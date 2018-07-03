#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "http.h"
#include "socket.h"
#include "dns.h"
#include "sds.h"
#include "url.h"
#include "http_parser.h"

static struct socket_server *default_server = NULL;
static http_parser_settings parser_settings;

//HTTP协议头定义
struct http_header {
    char *k;
    char *v;
    struct http_header *next;
};
//HTTP应答定义
struct http_response {
    http_parser parser;                     //http解析器
    struct http_header *header;    //请求头
};
//HTTP请求定义
struct http_request {
    //请求数据
    char *host;
    uint16_t port;

    uint8_t http_major;             //协议版本
    uint8_t http_minor;             //协议版本
    uint8_t ssl;                    //ssl协议
    uint8_t keep;                   //长连接
    char *method;                   //请求类型
    char *path;                     //请求路径
    struct http_header *header;     //请求头
    void *data;                     //提交数据
    uint32_t *dlen;                 //数据长度

    struct http_response *response; //应答数据

                                    //内核数据
    //HANDLE event;                   //同步事件
    uint8_t sync;                   //同步请求
    http_callback_end cb_end;       //完成回调
    http_callback_body cb_body;     //数据回调
    void *ud;                       //用户数据
    struct socket_server *server;   //执行请求的服务
};


static struct http_response *HTTP_Response_New(struct http_request *request) {
    struct http_response *response = (struct http_response *)malloc(sizeof(struct http_response));
    //准备解析器
    http_parser_init(&response->parser, HTTP_RESPONSE);
    response->parser.data = request;
    return response;
}
static void HTTP_Response_Delete(struct http_response *response) {


}
/* 异步回调处理 */
//事件_回收数据
static void __stdcall HTTP_CB_Free(struct socket_server * ss, sds s) {
    sdsfree(s);
}
//事件_接收
static void __stdcall HTTP_CB_Data(struct socket_server * ss, struct http_request *request, int state, char *data, uint32_t len) {
    if (!request->response)
        request->response = HTTP_Response_New(request);
    if (!request->response)
        return;
    uint32_t parsed = http_parser_execute(&request->response->parser, &parser_settings, data, len);
    if (parsed) {

    }
}
//事件_连接
static void __stdcall HTTP_CB_Connect(struct socket_server * ss, struct http_request *request, int state, int fd) {
    if (state) {
        //异常
    }
    //接收数据
    socket_tcp_start(ss, fd, HTTP_CB_Data, request);
    //生成数据
    sds s = sdscatprintf(sdsempty(),
        "%s /%s HTTP/%d.%d\r\n",
        request->method, request->path ? request->path : "", request->http_major, request->http_minor);
    //添加请求头
    struct http_header *header = request->header;
    while (header)
    {
        s = sdscatprintf(s,
            "%s: %s\r\n",
            header->k, header->v);
        header = header->next;
    }
    //请求结束
    s = sdscatprintf(s,
        "\r\n");
    //发送数据
    socket_tcp_send(ss, fd, s, sdslen(s), HTTP_CB_Free, s);
}
//事件_DNS
static void __stdcall HTTP_CB_Dns(struct socket_server * ss, struct http_request *request, int state, char *ip) {
    //获取ip
    if (state) {
        //异常

    }
    //连接服务器
    socket_tcp_connect(ss, ip, request->port, HTTP_CB_Connect, request);
}

//HTTP应答解析回调
//消息完毕
static int on_message_complete(http_parser *p) {
    struct http_request *request = (struct http_request *)p->data;
    if (request->cb_end)
        request->cb_end(request, request->ud);
    return 0;
}
//解析到消息体
static int on_body(http_parser *p, const char *buf, size_t len) {
    struct http_request *request = (struct http_request *)p->data;
    printf(buf);
    return 0;
}
//解析到头V
static int on_header_value(http_parser *p, const char *buf, size_t len) {
    struct http_request *request = (struct http_request *)p->data;
    
    return 0;
}
//解析到头K
static int on_header_field(http_parser *p, const char *buf, size_t len) {
    struct http_request *request = (struct http_request *)p->data;

    return 0;
}
//解析到url
static int on_url(http_parser *p, const char *buf, size_t len) {
    struct http_request *request = (struct http_request *)p->data;
    return 0;
}
//解析开始
static int on_message_begin(http_parser *p) {
    struct http_request *request = (struct http_request *)p->data;
    return 0;
}

//HTTP初始化
EXPORT int CALL HTTP_Init() {
    static uint32_t is_init = 0;
    if (is_init)
        return 1;

    socket_init();
    default_server = socket_new();

    //初始化http解析器
    memset(&parser_settings, 0, sizeof(parser_settings));

    parser_settings.on_message_complete = on_message_complete;
    parser_settings.on_body = on_body;
    parser_settings.on_header_value = on_header_value;
    parser_settings.on_header_field = on_header_field;
    parser_settings.on_url = on_url;
    parser_settings.on_message_begin = on_message_begin;

    is_init = 1;
    return 1;
}
//HTTP反初始化
EXPORT void CALL HTTP_UnInit() {

}

//创建HTTP请求
EXPORT struct http_request * CALL HTTP_Request_New(void *ud) {
    struct http_request *request = (struct http_request *)malloc(sizeof(struct http_request));
    if (!request)
        return NULL;
    memset(request, 0, sizeof(struct http_request));
    request->server = default_server;

    return request;
}
//销毁请求
EXPORT struct http_request * CALL HTTP_Request_Delete(struct http_request *request) {
    if (request->response)
        HTTP_Response_Delete(request->response);
    if (request->method)
        free(request->method);
    if (request->host)
        free(request->host);
    free(request);
}

//调整选项
EXPORT void CALL HTTP_Request_Option(struct http_request *request, char *k, char *v) {
    if (!request || !k || !v)
        return;

}
//设置请求头
EXPORT void CALL HTTP_Request_SetHeader(struct http_request *request, char *k, char *v) {
    if (!request || !k || !v)
        return;
    struct http_header *header = request->header;
    struct http_header *header_old = header;
    while (header)
    {
        if (strcmp(header->k, k) == 0) {
            if (header->v)
                free(header->v);
            header->v = strdup(v);
        }
        header_old = header;
        header = header->next;
    }
    if (header_old == NULL)
    {
        request->header = malloc(sizeof(struct http_header));
        request->header->k = strdup(k);
        request->header->v = strdup(v);
        request->header->next = NULL;
    }
    else {
        header_old->next = malloc(sizeof(struct http_header));
        header_old->next->k = strdup(k);
        header_old->next->v = strdup(v);
        header_old->next->next = NULL;
    }
}
//打开请求
EXPORT void CALL HTTP_Request_Open(struct http_request *request, char * method, char * url) {
    if (!request || !url)
        return;
    //解析地址
    url_field_t * u = url_parse(url);
    request->host = u->host;
    request->ssl = strcmp(u->schema, "https") == 0;
    if (u->port)
        request->port = u->port;
    else
        request->port = request->ssl ? 433 : 80;
    request->path = u->path;

    request->http_major = 1;
    request->http_minor = 1;
    request->method = strdup(method);

    HTTP_Request_SetHeader(request, "Host", request->host);
}
//发送请求
EXPORT void CALL HTTP_Request_Send(struct http_request *request) {
    if (!request)
        return;
    //为同步请求创建事件
    //if (request->event)
    //    request->event = CreateEventA(NULL, TRUE, FALSE, NULL);

    //解析DNS
    socket_dns(request->server, request->host, HTTP_CB_Dns, request);

    //同步请求等待事件
    //if (request->event) {
    //    WaitForSingleObject(request->event, INFINITE);
    //}
}



