/*
 * Author: xQmQ
 * Date: 2021/9/6 14:02
 * Description: HTTP 处理
 */

#ifndef QVER_INCLUDE_HTTP_EVENT_H_
#define QVER_INCLUDE_HTTP_EVENT_H_

#include "non-copyable.h"
#include "epoller.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

class HttpEvent : NonCopyable {
public:
public:
  explicit HttpEvent(int fd);

  ~HttpEvent() = default;

  void active();

  /*
   * description: 关闭客户连接
   * param: {}
   * return: {}
   */
  void shutdown();

  /*
   * description: 查看连接是否关闭
   * param: {}
   * return: {}
   */
  bool isShutdown() const;

  /*
   * description: 处理客户请求
   * param: {}
   * return: {}
   */
  void run();

  /*
   * description: 非阻塞读操作
   * param: {}
   * return: {}
   */
  bool read();

  /*
   * description: 非阻塞写操作
   * param: {}
   * return: {}
   */
  bool write();

public:
  static const int FILE_NAME_LENGTH_ = 200;   // 文件名最大长度
  static const int READ_BUFFER_SIZE_ = 2048;  // 读缓冲区大小
  static const int WRITE_BUFFER_SIZE_ = 1024; // 写缓冲区大小

  // 主状态机的三种状态
  enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0, // 当前正在分析请求行
    CHECK_STATE_HEADER,          // 当前正在分析头部字段
    CHECK_STATE_CONTENT          // 当前正在分析内容
  };

  // 从状态机读取行的三种状态
  enum LINE_STATUS {
    LINE_OK = 0, // 读取到一个完整行
    LINE_BAD,    // 行出错
    LINE_OPEN    // 行数据不完整
  };

  // HTTP 请求方法，仅支持 GET
  enum METHOD {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATCH
  };

  // 状态码
  enum HTTP_CODE {
    NO_REQUEST,        // 请求不完整，需要继续读取
    GET_REQUEST,       // 获得完整客户请求
    FILE_REQUEST,      // 200-客户请求文件成功
    BAD_REQUEST,       // 400-客户请求有语法错误
    FORBIDDEN_REQUEST, // 403-客户没有资源访问权限
    NO_RESOURCE,       // 404-客户请求资源不存在
    INTERNAL_ERROR,    // 500-服务器内部错误
    CLOSED_CONNECTION  // 客户端已经关闭连接
  };

private:
  void init();

  /*
   * description: 主状态机，分析 HTTP 请求的入口
   * param: {}
   * return: {}
   */
  HTTP_CODE runRead();

  /*
   * description: 根据 HTTP 请求处理结果，决定返回给客户端的信息
   * param: {}
   * return: {}
   */
  bool runWrite(HTTP_CODE result);

  /*
   * description: 从状态机，用于解析出一行内容
   * param: {}
   * return: {针对行分析得出的从状态机状态}
   */
  LINE_STATUS parseLine();

  /*
   * description: 获得读缓冲区中当前行起始位置
   * param: {}
   * return: {}
   */
  char *getLine();

  /*
   * description: 分析请求行，获得请求方法、URL 和 HTTP 版本号
   * param: {text->待分析的数据}
   * return: {}
   */
  HTTP_CODE parseRequestLine(char *text);

  /*
   * description: 分析头部字段
   * param: {text->待分析的数据}
   * return: {}
   */
  HTTP_CODE parseHeaders(char *text);

  /*
   * description: 判断是否完整读入 HTTP 请求的 entity-body，并没有进行解析
   * param: {text->待分析的数据}
   * return: {}
   */
  HTTP_CODE parseContent(char *text);

  /*
   * description: 当获取完整且正确的 HTTP 请求后，分析目标文件并做处理
   * param: {}
   * return: {}
   */
  HTTP_CODE doRequest();

  /*
   * description: 对内存映射区接触映射
   * param: {}
   * return: {}
   */
  void unmap();

  /*
   * description: 利用可变参数向写缓冲区中写入数据，以下函数通过次方法将相应信息写入写缓冲区
   * param: {}
   * return: {}
   */
  bool addResponse(const char *format, ...);

  /*
   * description: 应答报文中写入状态行
   * param: {status->状态码, title->状态短语}
   * return: {}
   */
  bool addStatusLine(int status, const char *title);

  /*
   * description: 应答报文的首部行中写入数据
   * param: {content_length->实体主体长度}
   * return: {}
   */
  bool addHeaders(int content_length);

  /*
   * description: 首部行中写入 Server
   * param: {}
   * return: {}
   */
  bool addServer(const char *server_name);

  /*
   * description: 首部行中写入实体主体长度
   * param: {content_length->实体主体长度}
   * return: {}
   */
  bool addConcentLength(int content_length);

  /*
   * description: 首部行中写入 Connection
   * param: {}
   * return: {}
   */
  bool addLinger();

  /*
   * description: 首部行中写入与实体主体间的空行
   * param: {}
   * return: {}
   */
  bool addBlankLine();

  /*
   * description: 应答报文中写入实体主体
   * param: {}
   * return: {}
   */
  bool addConetnt(const char *content);

private:
  int fd_; // 待处理的客户连接
  bool is_shutdown_;

  Epoller epoller_;

  char read_buffer_[READ_BUFFER_SIZE_]; // 读缓冲区
  int read_index_;                      // 读缓冲区中已经读入的客户数据的最后一个字符的下一个位置
  int checked_index_;                   // 当前正在分析的字符在读缓冲区中的位置
  int start_line_;                      // 当前正在解析的行的起始位置

  char write_buffer_[WRITE_BUFFER_SIZE_]; // 写缓冲区，用于记录应答报文的状态行和首部字段
  int write_index_;                       // 写缓冲区中带发送的字节数

  METHOD method_;                     // HTTP 请求方法
  char *url_;                         // 客户请求的目标文件的文件名
  char real_file_[FILE_NAME_LENGTH_]; // 客户请求的目标文件的完整路径
  char *http_version_;                // HTTP 协议版本号，仅支持 HTTP/1.1
  int content_length_;                // HTTP 请求的消息体的长度
  char *host_;                        // 主机名
  bool linger_;                       // 是否为长连接

  CHECK_STATE check_state_; // 主状态机当前所处的状态
  struct stat file_stat_;   // 目标文件状态
  char *file_address_;      // 客户请求的目标文件被 mmap 到内存中的起始位置，作为实体主体发送
  struct iovec iovec_[2];   // 通过 writev() 来执行写操作
  int iovec_count_;         // 被写内存块的数量
};

#endif //QVER_INCLUDE_HTTP_EVENT_H_
