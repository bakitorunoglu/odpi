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
// TestFetchDates.c
//   Tests simple fetch of dates.
//-----------------------------------------------------------------------------

#include "Test.h"
#define SQL_TEXT            "select * from TestTimestamps"

//-----------------------------------------------------------------------------
// main()
//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
    uint32_t numQueryColumns, bufferRowIndex, i;
    dpiData *intColValue, *timestampColValue;
    dpiNativeTypeNum nativeTypeNum;
    dpiTimestamp *timestamp;
    dpiQueryInfo queryInfo;
    dpiStmt *stmt;
    dpiConn *conn;
    int found;

    // connect to database
    conn = GetConnection(1, NULL);
    if (!conn)
        return -1;

    // prepare and execute statement
    if (dpiConn_prepareStmt(conn, 0, SQL_TEXT, strlen(SQL_TEXT), NULL, 0,
            &stmt) < 0)
        return ShowError();
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return ShowError();

    // fetch rows
    while (1) {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return ShowError();
        if (!found)
            break;
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &intColValue) < 0)
            return ShowError();
        printf("Row: IntCol = %g\n", intColValue->value.asDouble);
        for (i = 1; i < numQueryColumns; i++) {
            if (dpiStmt_getQueryValue(stmt, i + 1, &nativeTypeNum,
                    &timestampColValue) < 0)
                return ShowError();
            if (dpiStmt_getQueryInfo(stmt, i + 1, &queryInfo) < 0)
                return ShowError();
            printf("     %-18.*s = ", queryInfo.nameLength, queryInfo.name);
            if (timestampColValue->isNull)
                printf("null\n");
            else {
                timestamp = &timestampColValue->value.asTimestamp;
                printf("%4d-%.2d-%.2d %.2d:%.2d:%.2d.%.6d",
                        timestamp->year, timestamp->month, timestamp->day,
                        timestamp->hour, timestamp->minute, timestamp->second,
                        timestamp->fsecond);
                if (queryInfo.oracleTypeNum == DPI_ORACLE_TYPE_TIMESTAMP_TZ)
                    printf(" %+.2d:%.2d", timestamp->tzHourOffset,
                            timestamp->tzMinuteOffset);
                printf("\n");
            }
        }
    }

    // display description of each variable
    for (i = 0; i < numQueryColumns; i++) {
        if (dpiStmt_getQueryInfo(stmt, i + 1, &queryInfo) < 0)
            return ShowError();
        printf("('%*s', %d, %d, %d, %d, %d, %d)\n", queryInfo.nameLength,
                queryInfo.name, queryInfo.oracleTypeNum, queryInfo.sizeInChars,
                queryInfo.clientSizeInBytes, queryInfo.precision,
                queryInfo.scale, queryInfo.nullOk);
    }

    // clean up
    dpiStmt_release(stmt);
    dpiConn_release(conn);

    printf("Done.\n");
    return 0;
}

