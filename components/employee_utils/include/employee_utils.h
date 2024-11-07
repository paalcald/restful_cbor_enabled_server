#ifndef EMPLOYEE_UTILS_H
#define EMPLOYEE_UTILS_H

#include "cbor.h"

#define CBOR_MAP_ERR(a, goto_tag) \
    if ((a) != CborNoError)       \
    {                             \
        ret = a;                  \
        goto goto_tag;            \
    }

struct birthdate {
    int day;
    int month;
    int year;
};

struct employee {
    char name[256];
    double salary;
    struct birthdate birthdate;
};

size_t employee_toCBOR(const struct employee *e, uint8_t *buf, size_t len);
size_t employee_toJSON(const struct employee *e, char* buf, size_t len);
CborError cbor_encode_birthdate(CborEncoder * it, const struct birthdate *b);
CborError cbor_envode_employee(CborEncoder * it, const struct employee *e);
CborError cbor_value_get_employee(CborValue *it, struct employee *emp);
CborError cbor_value_get_birthdate(CborValue *it, struct birthdate *bdate);
CborError cbor_value_get_employee(CborValue *it, struct employee *emp);
#endif // EMPLOYEE_UTILS_H
