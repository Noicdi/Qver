/*
 * Author: xQmQ
 * Date: 2021/9/6 14:02
 * Description: HTTP 处理类的实现
 */

#include "../include/http-event.h"

// 定义 HTTP 响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "/html/400.html";
const char *error_403_title = "Forbidden";
const char *error_403_form = "/html/403.html";
const char *error_404_title = "Not Found";
const char *error_404_form = "/html/404.html";
const char *error_500_title = "Internal Error";
const char *error_500_form = "/html/500.html";
// 网站根目录，改进版通过配置文件定义
char *doc_root = "/home/z/Qver/html";

HttpEvent::HttpEvent(int fd) : fd_(fd)
{
  epoller_.addEvent(fd_, true, false, false);
  init();
}

void
HttpEvent::active()
{
  struct epoll_event events[MAX_EVENT_NUMBER];
  int number = 0;
  while (!isShutdown()) {
    number = epoller_.getFds(events);
    if (number != -1) {
      for (int i = 0; i < number; ++i) {
        if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
          epoller_.delEvent(events[i].data.fd, false);
          shutdown();
        }
        if (events[i].events & EPOLLIN) {
          if (read())
            run();
          else
            shutdown();
        }
        if (events[i].events & EPOLLOUT) {
          if (!write())
            shutdown();
        }
      }
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
  int length = 0;
  if (result != FILE_REQUEST) {
    char *cwd = getcwd(nullptr, 0);
    if (cwd) {
      strcpy(real_file_, cwd);
      length = strlen(real_file_);
    }
  }
  switch (result) {
    case INTERNAL_ERROR: {
      addStatusLine(500, error_500_title);
      strncpy(real_file_ + length, error_500_form, FILE_NAME_LENGTH_ - length - 1);
      if (openFile()) {
        addHeaders(file_stat_.st_size);
      } else {
        return false;
      }
      break;
    }
    case BAD_REQUEST: {
      addStatusLine(400, error_400_title);
      strncpy(real_file_ + length, error_400_form, FILE_NAME_LENGTH_ - length - 1);
      if (openFile()) {
        addHeaders(file_stat_.st_size);
      } else {
        return false;
      }
      break;
    }
    case NO_RESOURCE: {
      addStatusLine(404, error_404_title);
      strncpy(real_file_ + length, error_404_form, FILE_NAME_LENGTH_ - length - 1);
      if (openFile()) {
        addHeaders(file_stat_.st_size);
      } else {
        return false;
      }
      break;
    }
    case FORBIDDEN_REQUEST: {
      addStatusLine(403, error_403_title);
      strncpy(real_file_ + length, error_403_form, FILE_NAME_LENGTH_ - length - 1);
      if (openFile()) {
        addHeaders(file_stat_.st_size);
      } else {
        return false;
      }
      break;
    }
    case FILE_REQUEST: {
      addStatusLine(200, ok_200_title);
      if (openFile()) {
        addHeaders(file_stat_.st_size);
      } else {
        return false;
      }
      break;
    }
    default: {
      return false;
    }
  }

  iovec_[0].iov_base = write_buffer_;
  iovec_[0].iov_len = write_index_;
  iovec_[1].iov_base = file_address_;
  iovec_[1].iov_len = file_stat_.st_size;
  iovec_count_ = 2;
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
    return NO_RESOURCE;
  }
  if (!(file_stat_.st_mode & S_IROTH)) {
    return FORBIDDEN_REQUEST;
  }
  if (S_ISDIR(file_stat_.st_mode)) {
    // url_ 指向一个目录
    // 首先查看该目录下有无 index.html 文件
    // 没有则报 400
    char temp_file[FILE_NAME_LENGTH_];
    struct stat temp_stat;
    length = strlen(real_file_);
    strcpy(temp_file, real_file_);
    if (temp_file[length - 1] != '/') {
      strncpy(temp_file + length, "/", FILE_NAME_LENGTH_ - length - 1);
      ++length;
    }
    strncpy(temp_file + length, "index.html", FILE_NAME_LENGTH_ - length - 1);
    if ((stat(temp_file, &temp_stat) == 0) && (temp_stat.st_mode & S_IROTH)) {
      // 如果 index.html 文件存在且拥有读取权限，则设置 real_file_ 为 index.html
      strcpy(real_file_, temp_file);
      if (stat(real_file_, &file_stat_) < 0) {
        return NO_REQUEST;
      }
    } else {
      // 否则报 400
      return BAD_REQUEST;
    }
  }

  return FILE_REQUEST;
}

bool
HttpEvent::openFile()
{
  if (stat(real_file_, &file_stat_) < 0)
    return false;

  int fd = open(real_file_, O_RDONLY);
  file_address_ = (char *)mmap(nullptr, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  return true;
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