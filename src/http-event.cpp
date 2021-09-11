/*
 * Author: xQmQ
 * Date: 2021/9/6 14:02
 * Description:
 */

#include "../include/http-event.h"
#include <iostream>

// 定义 HTTP 响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the requested file.\n";
// 网站根目录，改进版通过配置文件定义
const char *doc_root = "/home/z/Qver/html";

/*
 * 1. 设置初始化函数，针对接收到的 fd 做状态检查
 *    - 如果处于连接状态，则初始化一系列成员变量，准备处理
 *    - 如果处于非活动状态，则调用结束函数
 *    虽然主线程剔除了异常连接，但是工作线程在接受任务过程中可能关闭
 *
 *  2. run() 调用 runRead() 完成 HTTP 解析
 *  3. run() 调用 runWrite() 完成发送数据
 *
 *  通过监控文件描述符
 *    - 如果读操作就绪，调用 read() 接收请求报文
 *      read() 成功则调用 run()
 *      read() 失败则调用 shutdown()
 *    - 如果写操作就绪，调用 write() 发送应答报文
 *      发送报文成功且是长连接，则监听其读事件
 *      发送报文成功且不是长连接
 *
 *
 */
HttpEvent::HttpEvent(int fd) : fd_(fd)
{
  epoller_.addEvent(fd_, true, false, true);
  init();
}

void
HttpEvent::active()
{
  std::vector<int> in_fd, out_fd, err_fd;
  while (!isShutdown()) {
    std::vector<int> in_fd, out_fd, err_fd;
    epoller_.getFds(&in_fd, &out_fd, &err_fd);
    if (!in_fd.empty()) {
      if (read()) {
        run();
      } else {
        shutdown();
      }
    } else if (!out_fd.empty()) {
      if (!write()) {
        shutdown();
      }
    }
    if (!err_fd.empty()) {
      is_shutdown_ = true;
    }
  }
}

void
HttpEvent::shutdown()
{
  is_shutdown_ = true;
  close(fd_);
  fd_ = -1;
}

bool
HttpEvent::isShutdown() const
{
  return is_shutdown_;
}

void
HttpEvent::run()
{
  HTTP_CODE read_result = runRead();
  if (read_result == NO_REQUEST) {
    epoller_.modEvent(fd_, EPOLLIN);
    return;
  }

  bool write_result = runWrite(read_result);
  if (!write_result) {
    shutdown();
    return;
  }
  epoller_.modEvent(fd_, EPOLLOUT);
}

bool
HttpEvent::read()
{
  if (read_index_ >= READ_BUFFER_SIZE_) {
    return false;
  }

  int bytes_read = 0;
  while (true) {
    bytes_read = recv(fd_, read_buffer_ + read_index_, READ_BUFFER_SIZE_ - read_index_, MSG_DONTWAIT);
    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      return false;
    } else if (bytes_read == 0) {
      return false;
    }
    read_index_ += bytes_read;
  }
  return true;
}

bool
HttpEvent::write()
{
  int temp = 0;
  int bytes_sent = 0;                  // 已发送字节数
  int bytes_to_be_send = write_index_; // 待发送字节数

  if (bytes_to_be_send == 0) {
    // 待发送字节已全部发送给客户，调整事件监控为读监控
    epoller_.modEvent(fd_, EPOLLIN);
    init();
    return true;
  }

  while (true) {
    temp = writev(fd_, iovec_, iovec_count_);
    if (temp <= -1) {
      // TCP 写缓冲没有空间，等待下一轮写就绪事件
      if (errno == EAGAIN) {
        epoller_.modEvent(fd_, EPOLLOUT);
        return true;
      }
      unmap();
      return false;
    }
    bytes_to_be_send -= temp;
    bytes_sent += temp;
    if (bytes_to_be_send <= bytes_sent) {
      unmap();
      // 发送 HTTP 响应报文成功，根据 HTTP 请求报文中 Connection 字段决定是否立即关闭连接
      if (linger_) {
        init();
        epoller_.modEvent(fd_, EPOLLIN);
        return true;
      } else {
        // epoller_.modEvent(fd_, EPOLLIN);
        return false;
      }
    }
  }
}

void
HttpEvent::init()
{
  is_shutdown_ = false;
  check_state_ = CHECK_STATE_REQUESTLINE;
  linger_ = false;
  method_ = GET;
  url_ = nullptr;
  http_version_ = nullptr;
  content_length_ = 0;
  host_ = nullptr;
  start_line_ = 0;
  checked_index_ = 0;
  read_index_ = 0;
  write_index_ = 0;

  memset(read_buffer_, '\0', READ_BUFFER_SIZE_);
  memset(write_buffer_, '\0', WRITE_BUFFER_SIZE_);
  memset(real_file_, '\0', FILE_NAME_LENGTH_);
}

HttpEvent::HTTP_CODE
HttpEvent::runRead()
{
  LINE_STATUS line_status = LINE_OK; // 记录当前行的读取状态
  HTTP_CODE result = NO_REQUEST;     // 记录 HTTP 请求的处理结果
  char *text = nullptr;

  // 主状态机，用于从读缓冲区中取出所有完整的行
  while (((check_state_ == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
      || (line_status = parseLine()) == LINE_OK) {
    text = getLine();             // 获取读缓冲区中的起始行
    start_line_ = checked_index_; // 记录下一行的起始位置
    printf("got 1 http line:%s\n", text);

    switch (check_state_) {
      case CHECK_STATE_REQUESTLINE: {
        // 分析请求行
        result = parseRequestLine(text);
        if (result == BAD_REQUEST) {
          return BAD_REQUEST;
        }
        break;
      }
      case CHECK_STATE_HEADER: {
        // 分析头部字段
        result = parseHeaders(text);
        // if (result == BAD_REQUEST) {
        //   return BAD_REQUEST;
        // } else if (result == GET_REQUEST) {
        //   return doRequest();
        if (result == GET_REQUEST) {
          return doRequest();
        }
        break;
      }
      case CHECK_STATE_CONTENT: {
        // 分析消息体
        result = parseContent(text);
        if (result == GET_REQUEST) {
          return doRequest();
        }
        line_status = LINE_OPEN;
        break;
      }
      default: {
        return INTERNAL_ERROR;
      }
    }
  }
  return NO_REQUEST;
}

bool
HttpEvent::runWrite(HttpEvent::HTTP_CODE result)
{
  switch (result) {
    case INTERNAL_ERROR: {
      addStatusLine(500, error_500_title);
      addHeaders(strlen(error_400_form));
      if (!addConetnt(error_500_form)) {
        return false;
      }
      break;
    }
    case BAD_REQUEST: {
      addStatusLine(400, error_400_title);
      addHeaders(strlen(error_400_form));
      if (!addConetnt(error_400_form)) {
        return false;
      }
      break;
    }
    case NO_RESOURCE: {
      addStatusLine(404, error_404_title);
      addHeaders(strlen(error_404_form));
      if (!addConetnt(error_404_form)) {
        return false;
      }
      break;
    }
    case FORBIDDEN_REQUEST: {
      addStatusLine(403, error_403_title);
      addHeaders(strlen(error_403_form));
      if (!addConetnt(error_403_form)) {
        return false;
      }
      break;
    }
    case FILE_REQUEST: {
      addStatusLine(200, ok_200_title);
      if (file_stat_.st_size != 0) {
        addHeaders(file_stat_.st_size);
        iovec_[0].iov_base = write_buffer_;
        iovec_[0].iov_len = write_index_;
        iovec_[1].iov_base = file_address_;
        iovec_[1].iov_len = file_stat_.st_size;
        iovec_count_ = 2;
        return true;
      } else {
        const char *ok_string = "<html><body></body></html>";
        addHeaders(strlen(ok_string));
        if (!addConetnt(ok_string)) {
          return false;
        }
      }
    }
    default: {
      return false;
    }
  }
  // 除状态码 200，其他状态码需要通过 iovec_传递数据
  iovec_[0].iov_base = write_buffer_;
  iovec_[0].iov_len = write_index_;
  iovec_count_ = 1;
  return true;
}

HttpEvent::LINE_STATUS
HttpEvent::parseLine()
{
  // HTTP 报文每一行利用 '\r\n' 作为结尾符
  // 此函数通过寻找结尾符，以此解析出一行内容
  for (; checked_index_ < read_index_; ++checked_index_) {
    if (read_buffer_[checked_index_] == '\r') {
      // '\r' 下一个字符不存在，表示数据行不完整
      if ((checked_index_ + 1) == read_index_) {
        return LINE_OPEN;
      } else if (read_buffer_[checked_index_ + 1] == '\n') {
        // '\r' 下一个字符为 '\n'，表示找到一个完整行
        read_buffer_[checked_index_++] = '\0';
        read_buffer_[checked_index_++] = '\0';
        return LINE_OK;
      }
      // 否则表示行出错
      return LINE_BAD;
    } else if (read_buffer_[checked_index_] == '\n') {
      if ((checked_index_ > 1) && (read_buffer_[checked_index_ - 1] == '\r')) {
        // 当前字符和上一个字符为 '\r\n'，表示找到一个完整行
        read_buffer_[checked_index_ - 1] = '\0';
        read_buffer_[checked_index_++] = '\0';
        return LINE_OK;
      }
      // 否则表示行出错
      return LINE_BAD;
    }
  }
  // 找遍读缓冲区且未找到，表示数据行不完整
  return LINE_OPEN;
}

char *
HttpEvent::getLine()
{
  return read_buffer_ + start_line_;
}

HttpEvent::HTTP_CODE
HttpEvent::parseRequestLine(char *text)
{
  // 检索 url_ 中是否存在 " \t"，如果不存在则返回 NULL
  url_ = strpbrk(text, " \t");
  // 如果不存在空白字符或者 '\t'，则 HTTP 请求报文出错
  if (!url_) {
    return BAD_REQUEST;
  }
  *url_++ = '\0';

  // 寻找请求方法，且只支持 GET 方法
  char *method = text;
  if (strcasecmp(method, "GET") == 0) {
    method_ = GET;
  } else {
    return BAD_REQUEST;
  }

  url_ += strspn(url_, " \t");
  http_version_ = strpbrk(url_, " \t");
  if (!http_version_) {
    return BAD_REQUEST;
  }

  *http_version_++ = '\0';
  http_version_ += strspn(http_version_, " \t");
  // 仅支持 HTTP/1.1
  if (strcasecmp(http_version_, "HTTP/1.1") != 0) {
    return BAD_REQUEST;
  }

  // 检查 URL 是否合法
  if (strncasecmp(url_, "http://", 7) == 0) {
    url_ += 7;
    url_ = strchr(url_, '/');
  } else if (strncasecmp(url_, "https://", 8) == 0) {
    url_ += 8;
    url_ = strchr(url_, '/');
  }

  if (!url_ || url_[0] != '/') {
    return BAD_REQUEST;
  }

  // HTTP 请求行处理完成，状态转移到头部字段的分析
  check_state_ = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

HttpEvent::HTTP_CODE
HttpEvent::parseHeaders(char *text)
{
  // 遇到空行，表示头部字段解析完毕
  if (text[0] == '\0') {
    // 如果 HTTP 请求有 entity-body
    // 需要读取 content_length_ 长度的消息体
    // 状态机转移状态
    if (content_length_ != 0) {
      check_state_ = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    // 否则说明得到完整的 HTTP 请求
    return GET_REQUEST;
  } else if (strncasecmp(text, "Connection:", 11) == 0) {
    // 处理 Connection 头部字段
    text += 11;
    text += strspn(text, " \t");
    if (strcasecmp(text, "keep-alive") == 0) {
      linger_ = true;
    }
  } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
    // 处理 Content-Length 头部字段
    text += 15;
    text += strspn(text, " \t");
    content_length_ = atol(text);
  } else if (strncasecmp(text, "HOST:", 5) == 0) {
    // 处理 HOST 头部字段
    text += 5;
    text += strspn(text, " \t");
    host_ = text;
  } else {
    printf("oop! unknow header %s\n", text);
  }
  return NO_REQUEST;
}

HttpEvent::HTTP_CODE
HttpEvent::parseContent(char *text)
{
  // 此函数实际上并没有处理实体，只是判断是否被完整读入
  if (read_index_ >= (content_length_ + checked_index_)) {
    text[content_length_] = '\0';
    return GET_REQUEST;
  }
  return NO_REQUEST;
}

HttpEvent::HTTP_CODE
HttpEvent::doRequest()
{
  strcpy(real_file_, doc_root);
  int length = strlen(doc_root);
  strncpy(real_file_ + length, url_, FILE_NAME_LENGTH_ - length - 1);
  if (stat(real_file_, &file_stat_) < 0) {
    return NO_REQUEST;
  }
  if (!(file_stat_.st_mode & S_IROTH)) {
    return FORBIDDEN_REQUEST;
  }
  if (S_ISDIR(file_stat_.st_mode)) {
    return BAD_REQUEST;
  }

  int fd = open(real_file_, O_RDONLY);
  file_address_ = (char *)mmap(nullptr, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);


  return FILE_REQUEST;
}

void
HttpEvent::unmap()
{
  if (file_address_) {
    munmap(file_address_, file_stat_.st_size);
    file_address_ = nullptr;
  }
}

bool
HttpEvent::addResponse(const char *format, ...)
{
  if (write_index_ >= WRITE_BUFFER_SIZE_) {
    return false;
  }

  va_list arg_list;
  va_start(arg_list, format);
  int length = vsnprintf(write_buffer_ + write_index_, WRITE_BUFFER_SIZE_ - 1 - write_index_, format, arg_list);
  if (length >= (WRITE_BUFFER_SIZE_ - 1 - write_index_)) {
    return false;
  }

  write_index_ += length;
  va_end(arg_list);
  return true;
}

bool
HttpEvent::addStatusLine(int status, const char *title)
{
  return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool
HttpEvent::addHeaders(int content_length)
{
  bool result = false;
  result = addServer("Qver");
  result = addConcentLength(content_length);
  result = addLinger();
  result = addBlankLine();
  return result;
}

bool
HttpEvent::addServer(const char *server_name)
{
  return addResponse("Server: %s\r\n", server_name);
}

bool
HttpEvent::addConcentLength(int content_length)
{
  return addResponse("Content-Length: %d\r\n", content_length);
}

bool
HttpEvent::addLinger()
{
  return addResponse("Connection: %s\r\n", linger_ ? "keep-alive" : "close");
}

bool
HttpEvent::addBlankLine()
{
  return addResponse("%s", "\r\n");
}

bool
HttpEvent::addConetnt(const char *content)
{
  return addResponse("%s", content);
}



