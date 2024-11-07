#include "employee_utils.h"
#include "esp_log.h"

static const char *TAG = "UTILS";

CborError cbor_value_get_birthdate(CborValue *it, struct birthdate *bdate)
{
    CborError ret;
    if (!cbor_value_is_map(it)) {
        ret = CborErrorImproperValue;
        goto err;
    }
    CborValue cbor_day;
    CBOR_MAP_ERR(cbor_value_map_find_value(it, "day", &cbor_day), err);
    CBOR_MAP_ERR(cbor_value_get_int(&cbor_day, &(bdate->day)), err);
    CborValue cbor_month;
    CBOR_MAP_ERR(cbor_value_map_find_value(it, "month", &cbor_month), err);
    CBOR_MAP_ERR(cbor_value_get_int(&cbor_month, &(bdate->month)), err);
    CborValue cbor_year;
    CBOR_MAP_ERR(cbor_value_map_find_value(it, "year", &cbor_year), err);
    CBOR_MAP_ERR(cbor_value_get_int(&cbor_year, &(bdate->year)), err);
    return CborNoError;
err:
    return ret;
}


CborError cbor_value_get_employee(CborValue *it, struct employee *emp)
{
    CborError ret;
    if (!cbor_value_is_map(it)) {
        return CborErrorImproperValue;
    }
    // Get the employee name
    CborValue name_it;
    CBOR_MAP_ERR(cbor_value_map_find_value(it, "name", &name_it), err);
    size_t name_len;
    CBOR_MAP_ERR(cbor_value_get_string_length(&name_it, &name_len), err);
    CBOR_MAP_ERR(cbor_value_copy_text_string(&name_it, (emp->name), &name_len, NULL), err);
    // Get the employee salary
    CborValue salary_it;
    CBOR_MAP_ERR(cbor_value_map_find_value(it, "salary", &salary_it), err);
    CBOR_MAP_ERR(cbor_value_get_double(&salary_it, &(emp->salary)), err);
    //Get the employee birthdate
    CborValue birthdate_it;
    CBOR_MAP_ERR(cbor_value_map_find_value(it, "birthdate", &birthdate_it), err);
    CBOR_MAP_ERR(cbor_value_get_birthdate(&birthdate_it, &(emp->birthdate)), err);
    cbor_value_advance(it);
    return CborNoError;
err:
    return ret;
}

size_t employee_toJSON(const struct employee *e, char *buf, size_t len)
{
    return snprintf(buf, len,
            "{\"name\":\"%s\",\"salary\":%.2f,\"birthdate\":{\"day\":%d,\"month\":%d,\"year\":%d}}",
            e->name, e->salary, e->birthdate.day, e->birthdate.month, e->birthdate.year);
}

CborError cbor_encode_birthdate(CborEncoder * it, const struct birthdate *b)
{
    CborError ret;
    CborEncoder attr;
    CBOR_MAP_ERR(cbor_encoder_create_map(it, &attr, 3), err);
    CBOR_MAP_ERR(cbor_encode_text_stringz(&attr, "day"), err);
    CBOR_MAP_ERR(cbor_encode_uint(&attr, b->day), err);
    CBOR_MAP_ERR(cbor_encode_text_stringz(&attr, "month"), err);
    CBOR_MAP_ERR(cbor_encode_uint(&attr, b->month), err);
    CBOR_MAP_ERR(cbor_encode_text_stringz(&attr, "year"), err);
    CBOR_MAP_ERR(cbor_encode_uint(&attr, b->year), err);
    CBOR_MAP_ERR(cbor_encoder_close_container(it, &attr), err);
    ESP_LOGI(TAG, "Birthdate encoded");
    return CborNoError;
err:
    return ret;
}

CborError cbor_encode_employee(CborEncoder *it, const struct employee *e)
{
    CborError ret;
    CborEncoder attr;
    CBOR_MAP_ERR(cbor_encoder_create_map(it, &attr, 3), err);
    CBOR_MAP_ERR(cbor_encode_text_stringz(&attr, "name"), err);
    CBOR_MAP_ERR(cbor_encode_text_stringz(&attr, e->name ), err);
    CBOR_MAP_ERR(cbor_encode_text_stringz(&attr, "salary"), err);
    CBOR_MAP_ERR(cbor_encode_double(&attr, e->salary), err);
    CBOR_MAP_ERR(cbor_encode_text_stringz(&attr, "birthdate"), err);
    CBOR_MAP_ERR(cbor_encode_birthdate(&attr, &(e->birthdate)), err);
    CBOR_MAP_ERR(cbor_encoder_close_container(it, &attr), err);
    ESP_LOGI(TAG, "Employee encoded");
    return CborNoError;
err:
    return ret;
}

size_t employee_toCBOR(const struct employee *e, uint8_t *buf, size_t len)
{
    ESP_LOGI(TAG, "attempting to encode employee");
    CborEncoder employee_encoder;
    cbor_encoder_init(&employee_encoder, buf, len, 0);
    cbor_encode_employee(&employee_encoder, e);
    return cbor_encoder_get_buffer_size(&employee_encoder, buf);
}
