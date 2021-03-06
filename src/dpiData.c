//-----------------------------------------------------------------------------
// Copyright (c) 2016 Oracle and/or its affiliates.  All rights reserved.
// This program is free software: you can modify it and/or redistribute it
// under the terms of:
//
// (i)  the Universal Permissive License v 1.0 or at your option, any
//      later version (http://oss.oracle.com/licenses/upl); and/or
//
// (ii) the Apache License v 2.0. (http://www.apache.org/licenses/LICENSE-2.0)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// dpiData.c
//   Implementation of transformation routines.
//-----------------------------------------------------------------------------

#include "dpiImpl.h"

// constants used for converting timestamps to/from an interval
#define DPI_MS_DAY        86400000  // 24 * 60 * 60 * 1000
#define DPI_MS_HOUR       3600000   // 60 * 60 * 1000
#define DPI_MS_MINUTE     60000     // 60 * 1000
#define DPI_MS_SECOND     1000      // ms per sec
#define DPI_MS_FSECOND    1000000   // 1000 * 1000


//-----------------------------------------------------------------------------
// dpiData__fromOracleDate() [INTERNAL]
//   Populate the data from an OCIDate structure.
//-----------------------------------------------------------------------------
int dpiData__fromOracleDate(dpiData *data, OCIDate *oracleValue)
{
    dpiTimestamp *timestamp = &data->value.asTimestamp;

    OCIDateGetDate(oracleValue, &timestamp->year, &timestamp->month,
            &timestamp->day);
    OCIDateGetTime(oracleValue, &timestamp->hour, &timestamp->minute,
            &timestamp->second);
    timestamp->fsecond = 0;
    timestamp->tzHourOffset = 0;
    timestamp->tzMinuteOffset = 0;
    return DPI_SUCCESS;
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleIntervalDS() [INTERNAL]
//   Populate the data from an OCIInterval structure (days/seconds).
//-----------------------------------------------------------------------------
int dpiData__fromOracleIntervalDS(dpiData *data, dpiEnv *env, dpiError *error,
        OCIInterval *oracleValue)
{
    dpiIntervalDS *interval = &data->value.asIntervalDS;
    sword status;

    status = OCIIntervalGetDaySecond(env->handle, error->handle,
            &interval->days, &interval->hours, &interval->minutes,
            &interval->seconds, &interval->fseconds, oracleValue);
    return dpiError__check(error, status, NULL, "get interval day/second");
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleIntervalYM() [INTERNAL]
//   Populate the data from an OCIInterval structure (years/months).
//-----------------------------------------------------------------------------
int dpiData__fromOracleIntervalYM(dpiData *data, dpiEnv *env, dpiError *error,
        OCIInterval *oracleValue)
{
    dpiIntervalYM *interval = &data->value.asIntervalYM;
    sword status;

    status = OCIIntervalGetYearMonth(env->handle, error->handle,
            &interval->years, &interval->months, oracleValue);
    return dpiError__check(error, status, NULL, "get interval year/month");
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleNumberAsDouble() [INTERNAL]
//   Populate the data from an OCINumber structure as a double.
//-----------------------------------------------------------------------------
int dpiData__fromOracleNumberAsDouble(dpiData *data, dpiEnv *env,
        dpiError *error, OCINumber *oracleValue)
{
    sword status;

    status = OCINumberToReal(error->handle, oracleValue, sizeof(double),
            &data->value.asDouble);
    return dpiError__check(error, status, NULL, "get double");
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleNumberAsInteger() [INTERNAL]
//   Populate the data from an OCINumber structure as an integer.
//-----------------------------------------------------------------------------
int dpiData__fromOracleNumberAsInteger(dpiData *data, dpiEnv *env,
        dpiError *error, OCINumber *oracleValue)
{
    sword status;

    status = OCINumberToInt(error->handle, oracleValue, sizeof(int64_t),
            OCI_NUMBER_SIGNED, &data->value.asInt64);
    return dpiError__check(error, status, NULL, "get integer");
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleNumberAsUnsignedInteger() [INTERNAL]
//   Populate the data from an OCINumber structure as an unsigned integer.
//-----------------------------------------------------------------------------
int dpiData__fromOracleNumberAsUnsignedInteger(dpiData *data, dpiEnv *env,
        dpiError *error, OCINumber *oracleValue)
{
    sword status;

    status = OCINumberToInt(error->handle, oracleValue, sizeof(uint64_t),
            OCI_NUMBER_UNSIGNED, &data->value.asUint64);
    return dpiError__check(error, status, NULL, "get unsigned integer");
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleNumberAsText() [INTERNAL]
//   Populate the data from an OCINumber structure as text.
//-----------------------------------------------------------------------------
int dpiData__fromOracleNumberAsText(dpiData *data, dpiVar *var, uint32_t pos,
        dpiError *error, OCINumber *oracleValue)
{
    uint8_t *target, numDigits, digits[DPI_NUMBER_MAX_DIGITS];
    int16_t decimalPointIndex, i;
    uint16_t *targetUtf16;
    dpiBytes *bytes;
    int isNegative;

    // parse the OCINumber structure
    if (dpiUtils__parseOracleNumber(oracleValue, &isNegative,
            &decimalPointIndex, &numDigits, digits, error) < 0)
        return DPI_FAILURE;

    // common initialization
    bytes = &data->value.asBytes;
    bytes->length = 0;

    // UTF-16 must be handled differently; the platform endianness is used in
    // order to be compatible with OCI which has this restriction
    if (var->env->charsetId == DPI_CHARSET_ID_UTF16) {
        targetUtf16 = (uint16_t*) bytes->ptr;

        // if negative, include the sign
        if (isNegative) {
            *targetUtf16++ = '-';
            bytes->length += 2;
        }

        // if the decimal point index is 0 or less, add the decimal point and
        // any leading zeroes that are needed
        if (decimalPointIndex <= 0) {
            *targetUtf16++ = '0';
            *targetUtf16++ = '.';
            bytes->length += (-decimalPointIndex + 2) * 2;
            for (; decimalPointIndex < 0; decimalPointIndex++)
                *targetUtf16++ = '0';
        }

        // add each of the digits
        for (i = 0; i < numDigits; i++) {
            if (i > 0 && i == decimalPointIndex) {
                *targetUtf16++ = '.';
                bytes->length += 2;
            }
            *targetUtf16++ = '0' + digits[i];
        }
        bytes->length += numDigits * 2;

        // if the decimal point index exceeds the number of digits, add any
        // trailing zeroes that are needed
        if (decimalPointIndex > numDigits) {
            for (i = numDigits; i < decimalPointIndex; i++)
                *targetUtf16++ = '0';
            bytes->length += (decimalPointIndex - numDigits) * 2;
        }

    // the following should be the same logic as the section above for UTF-16,
    // simply with single byte encodings instead
    } else {
        target = (uint8_t*) bytes->ptr;

        // if negative, include the sign
        bytes->length = 0;
        if (isNegative) {
            *target++ = '-';
            bytes->length++;
        }

        // if the decimal point index is 0 or less, add the decimal point and
        // any leading zeroes that are needed
        if (decimalPointIndex <= 0) {
            *target++ = '0';
            *target++ = '.';
            bytes->length += -decimalPointIndex + 2;
            for (; decimalPointIndex < 0; decimalPointIndex++)
                *target++ = '0';
        }

        // add each of the digits
        for (i = 0; i < numDigits; i++) {
            if (i > 0 && i == decimalPointIndex) {
                *target++ = '.';
                bytes->length++;
            }
            *target++ = '0' + digits[i];
        }
        bytes->length += numDigits;

        // if the decimal point index exceeds the number of digits, add any
        // trailing zeroes that are needed
        if (decimalPointIndex > numDigits) {
            for (i = numDigits; i < decimalPointIndex; i++)
                *target++ = '0';
            bytes->length += decimalPointIndex - numDigits;
        }

    }

    return DPI_SUCCESS;
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleTimestamp() [INTERNAL]
//   Populate the data from an OCIDateTime structure.
//-----------------------------------------------------------------------------
int dpiData__fromOracleTimestamp(dpiData *data, dpiEnv *env, dpiError *error,
        OCIDateTime *oracleValue, int withTZ)
{
    dpiTimestamp *timestamp = &data->value.asTimestamp;
    sword status;

    status = OCIDateTimeGetDate(env->handle, error->handle, oracleValue,
            &timestamp->year, &timestamp->month, &timestamp->day);
    if (dpiError__check(error, status, NULL, "get date portion") < 0)
        return DPI_FAILURE;
    status = OCIDateTimeGetTime(env->handle, error->handle, oracleValue,
            &timestamp->hour, &timestamp->minute, &timestamp->second,
            &timestamp->fsecond);
    if (dpiError__check(error, status, NULL, "get time portion") < 0)
        return DPI_FAILURE;
    if (withTZ) {
        status = OCIDateTimeGetTimeZoneOffset(env->handle, error->handle,
                oracleValue, &timestamp->tzHourOffset,
                &timestamp->tzMinuteOffset);
        if (dpiError__check(error, status, NULL, "get time zone portion") < 0)
            return DPI_FAILURE;
    } else {
        timestamp->tzHourOffset = 0;
        timestamp->tzMinuteOffset = 0;
    }
    return DPI_SUCCESS;
}


//-----------------------------------------------------------------------------
// dpiData__fromOracleTimestampAsDouble() [INTERNAL]
//   Populate the data from an OCIDateTime structure as a double value (number
// of milliseconds since January 1, 1970).
//-----------------------------------------------------------------------------
int dpiData__fromOracleTimestampAsDouble(dpiData *data, dpiEnv *env,
        dpiError *error, OCIDateTime *oracleValue)
{
    int32_t day, hour, minute, second, fsecond;
    OCIInterval *interval;
    sword status;

    // allocate interval to use in calculation
    status = OCIDescriptorAlloc(env->handle, (dvoid**) &interval,
            OCI_DTYPE_INTERVAL_DS, 0, 0);
    if (dpiError__check(error, status, NULL, "alloc interval") < 0)
        return DPI_FAILURE;

    // subtract dates to determine interval between date and base date
    status = OCIDateTimeSubtract(env->handle, error->handle, oracleValue,
            env->baseDate, interval);
    if (dpiError__check(error, status, NULL, "subtract date") < 0) {
        OCIDescriptorFree(interval, OCI_DTYPE_INTERVAL_DS);
        return DPI_FAILURE;
    }

    // get the days, hours, minutes and seconds from the interval
    status = OCIIntervalGetDaySecond(env->handle, error->handle, &day, &hour,
            &minute, &second, &fsecond, interval);
    OCIDescriptorFree(interval, OCI_DTYPE_INTERVAL_DS);
    if (dpiError__check(error, status, NULL, "get interval") < 0)
        return DPI_FAILURE;

    // calculate milliseconds since January 1, 1970
    data->value.asDouble = ((double) day) * DPI_MS_DAY + hour * DPI_MS_HOUR +
            minute * DPI_MS_MINUTE + second * DPI_MS_SECOND +
            fsecond / DPI_MS_FSECOND;
    return DPI_SUCCESS;
}


//-----------------------------------------------------------------------------
// dpiData__toOracleDate() [INTERNAL]
//   Populate the data in an OCIDate structure.
//-----------------------------------------------------------------------------
int dpiData__toOracleDate(dpiData *data, OCIDate *oracleValue)
{
    dpiTimestamp *timestamp = &data->value.asTimestamp;

    OCIDateSetDate(oracleValue, timestamp->year, timestamp->month,
            timestamp->day);
    OCIDateSetTime(oracleValue, timestamp->hour, timestamp->minute,
            timestamp->second);
    return DPI_SUCCESS;
}


//-----------------------------------------------------------------------------
// dpiData__toOracleIntervalDS() [INTERNAL]
//   Populate the data in an OCIInterval structure (days/seconds).
//-----------------------------------------------------------------------------
int dpiData__toOracleIntervalDS(dpiData *data, dpiEnv *env, dpiError *error,
        OCIInterval *oracleValue)
{
    dpiIntervalDS *interval = &data->value.asIntervalDS;
    sword status;

    status = OCIIntervalSetDaySecond(env->handle, error->handle,
            interval->days, interval->hours, interval->minutes,
            interval->seconds, interval->fseconds, oracleValue);
    return dpiError__check(error, status, NULL, "from interval day/second");
}


//-----------------------------------------------------------------------------
// dpiData__toOracleIntervalYM() [INTERNAL]
//   Populate the data in an OCIInterval structure (years/months).
//-----------------------------------------------------------------------------
int dpiData__toOracleIntervalYM(dpiData *data, dpiEnv *env, dpiError *error,
        OCIInterval *oracleValue)
{
    dpiIntervalYM *interval = &data->value.asIntervalYM;
    sword status;

    status = OCIIntervalSetYearMonth(env->handle, error->handle,
            interval->years, interval->months, oracleValue);
    return dpiError__check(error, status, NULL, "from interval year/month");
}


//-----------------------------------------------------------------------------
// dpiData__toOracleNumberFromDouble() [INTERNAL]
//   Populate the data in an OCINumber structure from a double.
//-----------------------------------------------------------------------------
int dpiData__toOracleNumberFromDouble(dpiData *data, dpiEnv *env,
        dpiError *error, OCINumber *oracleValue)
{
    sword status;

    status = OCINumberFromReal(error->handle, &data->value.asDouble,
            sizeof(double), oracleValue);
    return dpiError__check(error, status, NULL, "from double");
}


//-----------------------------------------------------------------------------
// dpiData__toOracleNumberFromInteger() [INTERNAL]
//   Populate the data in an OCINumber structure from an integer.
//-----------------------------------------------------------------------------
int dpiData__toOracleNumberFromInteger(dpiData *data, dpiEnv *env,
        dpiError *error, OCINumber *oracleValue)
{
    sword status;

    status = OCINumberFromInt(error->handle, &data->value.asInt64,
            sizeof(int64_t), OCI_NUMBER_SIGNED, oracleValue);
    return dpiError__check(error, status, NULL, "from integer");
}


//-----------------------------------------------------------------------------
// dpiData__toOracleNumberFromText() [INTERNAL]
//   Populate the data in an OCINumber structure from text.
//-----------------------------------------------------------------------------
int dpiData__toOracleNumberFromText(dpiData *data, dpiEnv *env,
        dpiError *error, OCINumber *oracleValue)
{
    uint8_t numDigits, digits[DPI_NUMBER_AS_TEXT_CHARS], *source, *target, i;
    int isNegative, prependZero, appendSentinel;
    dpiBytes *value = &data->value.asBytes;
    int16_t decimalPointIndex;
    uint8_t byte, numPairs;
    int8_t ociExponent;

    // parse the string into its constituent components
    if (dpiUtils__parseNumberString(value->ptr, value->length, env->charsetId,
            &isNegative, &decimalPointIndex, &numDigits, digits, error) < 0)
        return DPI_FAILURE;

    // if the exponent is odd, prepend a zero
    prependZero = (decimalPointIndex > 0 && decimalPointIndex % 2 == 1) ||
            (decimalPointIndex < 0 && decimalPointIndex % 2 == -1);
    if (prependZero) {
        numDigits++;
        decimalPointIndex++;
    }

    // append a sentinel 102 byte for negative numbers if there is room
    appendSentinel = (isNegative && numDigits < DPI_NUMBER_MAX_DIGITS);

    // determine the number of digit pairs; if the number of digits is odd,
    // append a zero to make the number of digits even
    if (numDigits % 2 == 1)
        digits[numDigits++] = 0;
    numPairs = numDigits / 2;

    // initialize the OCINumber value
    // the length is the number of pairs, plus one for the exponent
    // include an extra byte for the sentinel if applicable
    target = (uint8_t*) oracleValue;
    *target++ = numPairs + 1 + appendSentinel;

    // if the number of digits is zero, the value is itself zero since all
    // leading and trailing zeroes are removed from the digits string; the OCI
    // value for zero is a special case
    if (numDigits == 0) {
        *target = 128;
        return DPI_SUCCESS;
    }

    // calculate the exponent
    ociExponent = (decimalPointIndex - 2) / 2 + 193;
    if (isNegative)
        ociExponent = ~ociExponent;
    *target++ = ociExponent;

    // calculate the mantissa bytes
    source = digits;
    for (i = 0; i < numPairs; i++) {
        if (i == 0 && prependZero)
            byte = *source++;
        else {
            byte = *source++ * 10;
            byte += *source++;
        }
        if (isNegative)
            byte = 101 - byte;
        else byte++;
        *target++ = byte;
    }

    // append 102 byte for negative numbers if the number of digits is less
    // than the maximum allowable
    if (appendSentinel)
        *target = 102;

    return DPI_SUCCESS;
}


//-----------------------------------------------------------------------------
// dpiData__toOracleNumberFromUnsignedInteger() [INTERNAL]
//   Populate the data in an OCINumber structure from an integer.
//-----------------------------------------------------------------------------
int dpiData__toOracleNumberFromUnsignedInteger(dpiData *data, dpiEnv *env,
        dpiError *error, OCINumber *oracleValue)
{
    sword status;

    status = OCINumberFromInt(error->handle, &data->value.asUint64,
            sizeof(uint64_t), OCI_NUMBER_UNSIGNED, oracleValue);
    return dpiError__check(error, status, NULL, "from unsigned integer");
}


//-----------------------------------------------------------------------------
// dpiData__toOracleTimestamp() [INTERNAL]
//   Populate the data in an OCIDateTime structure.
//-----------------------------------------------------------------------------
int dpiData__toOracleTimestamp(dpiData *data, dpiEnv *env, dpiError *error,
        OCIDateTime *oracleValue, int withTZ)
{
    dpiTimestamp *timestamp = &data->value.asTimestamp;
    char tzOffsetBuffer[10], *tzOffset = NULL;
    size_t tzOffsetLength = 0;
    sword status;
    uword valid;

    if (withTZ) {
        (void) sprintf(tzOffsetBuffer, "%+.2d:%.2d", timestamp->tzHourOffset,
                timestamp->tzMinuteOffset);
        tzOffset = tzOffsetBuffer;
        tzOffsetLength = strlen(tzOffset);
    }
    status = OCIDateTimeConstruct(env->handle, error->handle,
            oracleValue, timestamp->year, timestamp->month, timestamp->day,
            timestamp->hour, timestamp->minute, timestamp->second,
            timestamp->fsecond, (OraText*) tzOffset, tzOffsetLength);
    if (dpiError__check(error, status, NULL, "construct date") < 0)
        return DPI_FAILURE;
    status = OCIDateTimeCheck(env->handle, error->handle, oracleValue,
            &valid);
    if (dpiError__check(error, status, NULL, "check validity") < 0)
        return DPI_FAILURE;
    if (valid != 0)
        return dpiError__set(error, "check date", DPI_ERR_INVALID_DATE);
    return DPI_SUCCESS;
}


//-----------------------------------------------------------------------------
// dpiData__toOracleTimestampFromDouble() [INTERNAL]
//   Populate the data in an OCIDateTime structure, given the number of
// milliseconds since January 1, 1970.
//-----------------------------------------------------------------------------
int dpiData__toOracleTimestampFromDouble(dpiData *data, dpiEnv *env,
        dpiError *error, OCIDateTime *oracleValue)
{
    int32_t day, hour, minute, second, fsecond;
    OCIInterval *interval;
    sword status;
    double ms;

    // allocate interval to use in calculation
    status = OCIDescriptorAlloc(env->handle, (dvoid**) &interval,
            OCI_DTYPE_INTERVAL_DS, 0, 0);
    if (dpiError__check(error, status, NULL, "alloc interval") < 0)
        return DPI_FAILURE;

    // determine the interval
    ms = data->value.asDouble;
    day = (int32_t) (ms / DPI_MS_DAY);
    ms = ms - ((double) day) * DPI_MS_DAY;
    hour = (int32_t) (ms / DPI_MS_HOUR);
    ms = ms - (hour * DPI_MS_HOUR);
    minute = (int32_t) (ms / DPI_MS_MINUTE);
    ms = ms - (minute * DPI_MS_MINUTE);
    second = (int32_t) (ms / DPI_MS_SECOND);
    ms = ms - (second * DPI_MS_SECOND);
    fsecond = (int32_t)(ms * DPI_MS_FSECOND);
    status = OCIIntervalSetDaySecond(env->handle, error->handle, day, hour,
            minute, second, fsecond, interval);
    if (dpiError__check(error, status, NULL, "get interval") < 0) {
        OCIDescriptorFree(interval, OCI_DTYPE_INTERVAL_DS);
        return DPI_FAILURE;
    }

    // add the interval to the base date
    status = OCIDateTimeIntervalAdd(env->handle, error->handle, env->baseDate,
            interval, oracleValue);
    OCIDescriptorFree(interval, OCI_DTYPE_INTERVAL_DS);
    return dpiError__check(error, status, NULL, "add date");
}


//-----------------------------------------------------------------------------
// dpiData_getBool() [PUBLIC]
//   Return the boolean portion of the data.
//-----------------------------------------------------------------------------
int dpiData_getBool(dpiData *data)
{
    return data->value.asBoolean;
}


//-----------------------------------------------------------------------------
// dpiData_getBytes() [PUBLIC]
//   Return the bytes portion of the data.
//-----------------------------------------------------------------------------
dpiBytes *dpiData_getBytes(dpiData *data)
{
    return &data->value.asBytes;
}


//-----------------------------------------------------------------------------
// dpiData_getDouble() [PUBLIC]
//   Return the double portion of the data.
//-----------------------------------------------------------------------------
double dpiData_getDouble(dpiData *data)
{
    return data->value.asDouble;
}


//-----------------------------------------------------------------------------
// dpiData_getFloat() [PUBLIC]
//   Return the float portion of the data.
//-----------------------------------------------------------------------------
float dpiData_getFloat(dpiData *data)
{
    return data->value.asFloat;
}


//-----------------------------------------------------------------------------
// dpiData_getInt64() [PUBLIC]
//   Return the integer portion of the data.
//-----------------------------------------------------------------------------
int64_t dpiData_getInt64(dpiData *data)
{
    return data->value.asInt64;
}


//-----------------------------------------------------------------------------
// dpiData_getIntervalDS() [PUBLIC]
//   Return the interval (days/seconds) portion of the data.
//-----------------------------------------------------------------------------
dpiIntervalDS *dpiData_getIntervalDS(dpiData *data)
{
    return &data->value.asIntervalDS;
}


//-----------------------------------------------------------------------------
// dpiData_getIntervalYM() [PUBLIC]
//   Return the interval (years/months) portion of the data.
//-----------------------------------------------------------------------------
dpiIntervalYM *dpiData_getIntervalYM(dpiData *data)
{
    return &data->value.asIntervalYM;
}


//-----------------------------------------------------------------------------
// dpiData_getLOB() [PUBLIC]
//   Return the LOB portion of the data.
//-----------------------------------------------------------------------------
dpiLob *dpiData_getLOB(dpiData *data)
{
    return data->value.asLOB;
}


//-----------------------------------------------------------------------------
// dpiData_getObject() [PUBLIC]
//   Return the object portion of the data.
//-----------------------------------------------------------------------------
dpiObject *dpiData_getObject(dpiData *data)
{
    return data->value.asObject;
}


//-----------------------------------------------------------------------------
// dpiData_getStmt() [PUBLIC]
//   Return the statement portion of the data.
//-----------------------------------------------------------------------------
dpiStmt *dpiData_getStmt(dpiData *data)
{
    return data->value.asStmt;
}


//-----------------------------------------------------------------------------
// dpiData_getTimestamp() [PUBLIC]
//   Return the timestamp portion of the data.
//-----------------------------------------------------------------------------
dpiTimestamp *dpiData_getTimestamp(dpiData *data)
{
    return &data->value.asTimestamp;
}


//-----------------------------------------------------------------------------
// dpiData_getUint64() [PUBLIC]
//   Return the unsigned integer portion of the data.
//-----------------------------------------------------------------------------
uint64_t dpiData_getUint64(dpiData *data)
{
    return data->value.asUint64;
}


//-----------------------------------------------------------------------------
// dpiData_setBool() [PUBLIC]
//   Set the boolean portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setBool(dpiData *data, int value)
{
    data->isNull = 0;
    data->value.asBoolean = value;
}


//-----------------------------------------------------------------------------
// dpiData_setBytes() [PUBLIC]
//   Set the bytes portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setBytes(dpiData *data, char *ptr, uint32_t length)
{
    data->isNull = 0;
    data->value.asBytes.ptr = ptr;
    data->value.asBytes.length = length;
}


//-----------------------------------------------------------------------------
// dpiData_setDouble() [PUBLIC]
//   Set the double portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setDouble(dpiData *data, double value)
{
    data->isNull = 0;
    data->value.asDouble = value;
}


//-----------------------------------------------------------------------------
// dpiData_setFloat() [PUBLIC]
//   Set the float portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setFloat(dpiData *data, float value)
{
    data->isNull = 0;
    data->value.asFloat = value;
}


//-----------------------------------------------------------------------------
// dpiData_setInt64() [PUBLIC]
//   Set the integer portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setInt64(dpiData *data, int64_t value)
{
    data->isNull = 0;
    data->value.asInt64 = value;
}


//-----------------------------------------------------------------------------
// dpiData_setIntervalDS() [PUBLIC]
//   Set the interval (days/seconds) portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setIntervalDS(dpiData *data, int32_t days, int32_t hours,
        int32_t minutes, int32_t seconds, int32_t fseconds)
{
    dpiIntervalDS *interval = &data->value.asIntervalDS;

    data->isNull = 0;
    interval->days = days;
    interval->hours = hours;
    interval->minutes = minutes;
    interval->seconds = seconds;
    interval->fseconds = fseconds;
}


//-----------------------------------------------------------------------------
// dpiData_setIntervalYM() [PUBLIC]
//   Set the interval (years/months) portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setIntervalYM(dpiData *data, int32_t years, int32_t months)
{
    dpiIntervalYM *interval = &data->value.asIntervalYM;

    data->isNull = 0;
    interval->years = years;
    interval->months = months;
}


//-----------------------------------------------------------------------------
// dpiData_setLOB() [PUBLIC]
//   Set the LOB portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setLOB(dpiData *data, dpiLob *lob)
{
    data->isNull = 0;
    data->value.asLOB = lob;
}


//-----------------------------------------------------------------------------
// dpiData_setObject() [PUBLIC]
//   Set the object portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setObject(dpiData *data, dpiObject *obj)
{
    data->isNull = 0;
    data->value.asObject = obj;
}


//-----------------------------------------------------------------------------
// dpiData_setStmt() [PUBLIC]
//   Set the statement portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setStmt(dpiData *data, dpiStmt *obj)
{
    data->isNull = 0;
    data->value.asStmt = obj;
}


//-----------------------------------------------------------------------------
// dpiData_setTimestamp() [PUBLIC]
//   Set the timestamp portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setTimestamp(dpiData *data, int16_t year, uint8_t month,
        uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
        uint32_t fsecond, int8_t tzHourOffset, int8_t tzMinuteOffset)
{
    dpiTimestamp *timestamp = &data->value.asTimestamp;

    data->isNull = 0;
    timestamp->year = year;
    timestamp->month = month;
    timestamp->day = day;
    timestamp->hour = hour;
    timestamp->minute = minute;
    timestamp->second = second;
    timestamp->fsecond = fsecond;
    timestamp->tzHourOffset = tzHourOffset;
    timestamp->tzMinuteOffset = tzMinuteOffset;
}


//-----------------------------------------------------------------------------
// dpiData_setUint64() [PUBLIC]
//   Set the unsigned integer portion of the data.
//-----------------------------------------------------------------------------
void dpiData_setUint64(dpiData *data, uint64_t value)
{
    data->isNull = 0;
    data->value.asUint64 = value;
}

