/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "cbor.h"
#include "employee_utils.h"

static const char *REST_TAG = "esp-rest";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
do                                                                                 \
{                                                                                  \
    if (!(a))                                                                      \
    {                                                                              \
        ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        goto goto_tag;                                                             \
    }                                                                              \
} while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;


#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Simple handler for light brightness control */
static esp_err_t light_brightness_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    int red = cJSON_GetObjectItem(root, "red")->valueint;
    int green = cJSON_GetObjectItem(root, "green")->valueint;
    int blue = cJSON_GetObjectItem(root, "blue")->valueint;
    ESP_LOGI(REST_TAG, "Light control: red = %d, green = %d, blue = %d", red, green, blue);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t employee_cbor_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    CborParser root_parser;
    CborValue it;
    cbor_parser_init((uint8_t*) buf, total_len, 0, &root_parser, &it);
    struct employee employee;
    CborError ret = cbor_value_get_employee(&it, &employee);
    char employee_json[256];
    employee_toJSON(&employee, employee_json, 256);
    ESP_LOGI(REST_TAG, "Received employee:\n %s", employee_json);
    httpd_resp_sendstr(req, "Request to add employee received");
    return (ret == CborNoError) ? ESP_OK : ESP_FAIL;
}

static esp_err_t employee_post_handler(httpd_req_t *req)
{
    int ret = 0;
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    char *name = calloc(256, sizeof(char));
    cJSON *name_json = cJSON_GetObjectItem(root, "name");
    if (cJSON_IsString(name_json)) {
        strncpy(name, name_json->valuestring, 256);
        name[255] = '\0';
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wrong format");
        ret = -1;
        goto cleanup;
    }
    double salary = 0;
    cJSON *salary_json= cJSON_GetObjectItem(root, "salary");
    if (cJSON_IsNumber(salary_json)) {
        salary = salary_json->valuedouble;
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wrong format");
        ret = -1;
        goto cleanup;
    }
    int day = 0;
    int month = 0;
    int year = 0;
    cJSON *birthdate_json = cJSON_GetObjectItem(root, "birthdate");
    if (cJSON_IsObject(birthdate_json)) {
        cJSON *day_json = cJSON_GetObjectItem(birthdate_json, "day");
        if (cJSON_IsNumber(day_json)) {
            day = day_json->valueint;
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wrong day format");
            ret = -1;
            goto cleanup;
        }
        cJSON *month_json = cJSON_GetObjectItem(birthdate_json, "month");
        if (cJSON_IsNumber(month_json)) {
            month = month_json->valueint;
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wrong month format");
            ret = -1;
            goto cleanup;
        }
        cJSON *year_json = cJSON_GetObjectItem(birthdate_json, "year");
        if (cJSON_IsNumber(year_json)) {
            year = year_json->valueint;
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wrong year format");
            ret = -1;
            goto cleanup;
        }
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wrong birthdate format");
        ret = -1;
        goto cleanup;
    }
    ESP_LOGI(REST_TAG, "Received Employee: name = %s, salary = %.2f, birthdate = %d/%d/%d", name, salary, day, month, year);
    httpd_resp_sendstr(req, "Request to add employee received");
    goto cleanup;

cleanup:
    cJSON_Delete(root);
    free(name);
    return ret ? ESP_FAIL : ESP_OK;
}
/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Simple handler for getting temperature data */
static esp_err_t temperature_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "raw", esp_random() % 20);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t example_employee_cbor_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/cbor");
    struct employee example_emp;
    strcpy(example_emp.name, "John Doe");
    example_emp.salary = 1200.0;
    example_emp.birthdate.day = 1;
    example_emp.birthdate.month = 1;
    example_emp.birthdate.year = 2000;
    uint8_t buf[256];
    size_t len = employee_toCBOR(&example_emp, buf, 256);
    httpd_resp_send(req, (char*) buf, len);
    return ESP_OK;
}

double to_fahrenheit(double celsius)
{
    return (1.8 * celsius + 32.0);
}

static esp_err_t temperature_f_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "fahrenheit", to_fahrenheit((double) (esp_random() % 20)));
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/v1/system/info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    /* URI handler for fetching example employee data */
    httpd_uri_t example_employee_cbor_uri = {
        .uri = "/api/v2/employee/example",
        .method = HTTP_GET,
        .handler = example_employee_cbor_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &example_employee_cbor_uri);
    /* URI handler for fetching temperature data */
    httpd_uri_t temperature_data_get_uri = {
        .uri = "/api/v1/temp/raw",
        .method = HTTP_GET,
        .handler = temperature_data_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &temperature_data_get_uri);

    /* URI handler for fetching temperature data in fahrenheit*/
    httpd_uri_t temperature_f_get_uri = {
        .uri = "/api/v1/temp/fahrenheit",
        .method = HTTP_GET,
        .handler = temperature_f_data_get_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &temperature_f_get_uri);
    /* URI handler for light brightness control */
    httpd_uri_t light_brightness_post_uri = {
        .uri = "/api/v1/light/brightness",
        .method = HTTP_POST,
        .handler = light_brightness_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &light_brightness_post_uri);

    /* URI handler for employee addition */
    httpd_uri_t employee_post_uri = {
        .uri = "/api/v1/employee/add",
        .method = HTTP_POST,
        .handler = employee_post_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &employee_post_uri);
    /* URI handler for employee addition using cbor*/
    httpd_uri_t employee_cbor_post_uri = {
        .uri = "/api/v2/employee/add",
        .method = HTTP_POST,
        .handler = employee_cbor_post_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &employee_cbor_post_uri);
    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
