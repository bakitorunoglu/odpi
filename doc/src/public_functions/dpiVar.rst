.. _dpiVarFunctions:

******************
Variable Functions
******************

Variable handles are used to represent memory areas used for transferring data
to and from the database. They are created by calling the function
:func:`dpiConn_newVar()`. They are destroyed when the last reference to the
variable is released by calling the function :func:`dpiVar_release()`. They are
bound to statements by calling the function :func:`dpiStmt_bindByName()` or the
function :func:`dpiStmt_bindByPos()`. They can also be used for fetching data
from the database by calling the function :func:`dpiStmt_define()`.

.. function:: int dpiVar_addRef(dpiVar \*var)

    Adds a reference to the variable. This is intended for situations where a
    reference to the variable needs to be maintained independently of the
    reference returned when the variable was created.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- the variable to which a reference is to be added. If the
    reference is NULL or invalid an error is returned.


.. function:: int dpiVar_copyData(dpiVar \*var, uint32_t pos, \
        dpiVar \*sourceVar, uint32_t sourcePos)

    Copies the data from one variable to another variable.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- the variable into which data is to be copied. If the reference
    is NULL or invalid an error is returned.

    **pos** -- the array position into which the data is to be copied. The
    first position is 0. If the array position specified exceeds the number of
    elements allocated in the variable, an error is returned.

    **sourceVar** -- the variable from which is to be copied. If the reference
    is NULL or invalid an error is returned.

    **sourcePos** -- the array position from which the data is to be copied.
    The first position is 0. If the array position specified exceeds the number
    of elements allocated in the source variable, an error is returned.


.. function:: int dpiVar_getData(dpiVar \*var, uint32_t \*numElements, \
        dpiData \**data)

    Returns a pointer to an array of :ref:`dpiData` structures used for
    transferring data to and from the database. These structures are allocated
    by the variable itself and are made available when the variable is first
    created using the function :func:`dpiConn_newVar()`. If a DML returning
    statement is executed, however, the number of allocated elements can change
    in addition to the memory location.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable which contains the data structures
    used for transferring data to and from the database. If the reference is
    NULL or invalid an error is returned.

    **numElements** -- a pointer to the number of elements that have been
    allocated by the variable, which will be populated when the function
    completes successfully.

    **data** -- a pointer to an array of :ref:`dpiData` structures which will
    be populated when the function completes successfully.


.. function:: int dpiVar_getNumElementsInArray(dpiVar \*var, \
        uint32_t \*numElements)

    Returns the number of elements in a PL/SQL index-by table.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable from which the number of elements is
    to be retrieved. If the reference is NULL or invalid an error is returned.

    **numElements** -- a pointer to the number of elements, which will be
    populated when the function completes successfully.


.. function:: int dpiVar_getSizeInBytes(dpiVar \*var, uint32_t \*sizeInBytes)

    Returns the size of the buffer used for one element of the array used for
    fetching/binding Oracle data.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable whose buffer size is to be
    retrieved. If the reference is NULL or invalid an error is returned.

    **sizeInBytes** -- a pointer to the size of the buffer, in bytes, which
    which will be populated when the function completes successfully.


.. function:: int dpiVar_release(dpiVar \*var)

    Releases a reference to the variable. A count of the references to the
    variable is maintained and when this count reaches zero, the memory
    associated with the variable is freed.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- the variable from which a reference is to be released. If
    the reference is NULL or invalid an error is returned.


.. function:: int dpiVar_resize(dpiVar \*var, uint32_t sizeInBytes)

    Resizes the variable so that a different size in bytes can be accommodated
    for transferring data to and from the database.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable which is to be resized. If the
    reference is NULL or invalid an error is returned.

    **sizeInBytes** -- the new size in bytes to use for each element allocated
    in the variable.


.. function:: int dpiVar_setFromLob(dpiVar \*var, uint32_t pos, dpiLob \*lob)

    Sets the variable value to the specified LOB.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable which should be set. If the
    reference is null or invalid an error is returned.

    **pos** -- the array position in the variable which is to be set. The first
    position is 0. If the position exceeds the number of elements allocated by
    the variable an error is returned.

    **lob** -- a reference to the LOB which should be set. If the reference is
    null or invalid an error is returned. A reference is retained by the
    variable until a new value is set or the variable itself is freed.


.. function:: int dpiVar_setFromObject(dpiVar \*var, uint32_t pos, \
        dpiObject \*obj)

    Sets the variable value to the specified object.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable which should be set. If the
    reference is null or invalid an error is returned.

    **pos** -- the array position in the variable which is to be set. The first
    position is 0. If the position exceeds the number of elements allocated by
    the variable an error is returned.

    **obj** -- a reference to the object which should be set. If the reference
    is null or invalid an error is returned. A reference is retained by the
    variable until a new value is set or the variable itself is freed.


.. function:: int dpiVar_setFromRowid(dpiVar \*var, uint32_t pos, \
        dpiRowid \*rowid)

    Sets the variable value to the specified rowid.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable which should be set. If the
    reference is null or invalid an error is returned.

    **pos** -- the array position in the variable which is to be set. The first
    position is 0. If the position exceeds the number of elements allocated by
    the variable an error is returned.

    **stmt** -- a reference to the rowid which should be set. If the reference
    is null or invalid an error is returned. A reference is retained by the
    variable until a new value is set or the variable itself is freed.


.. function:: int dpiVar_setFromStmt(dpiVar \*var, uint32_t pos, \
        dpiStmt \*stmt)

    Sets the variable value to the specified statement.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable which should be set. If the
    reference is null or invalid an error is returned.

    **pos** -- the array position in the variable which is to be set. The first
    position is 0. If the position exceeds the number of elements allocated by
    the variable an error is returned.

    **stmt** -- a reference to the statement which should be set. If the
    reference is null or invalid an error is returned. A reference is retained
    by the variable until a new value is set or the variable itself is freed.


.. function:: int dpiVar_setNumElementsInArray(dpiVar \*var, \
        uint32_t numElements)

    Sets the number of elements in a PL/SQL index-by table.

    The function returns DPI_SUCCESS for success and DPI_FAILURE for failure.

    **var** -- a reference to the variable in which the number of elements is
    to be set. If the reference is NULL or invalid an error is returned.

    **numElements** -- the number of elements that PL/SQL should consider part
    of the array. This number should not exceed the number of elements that
    have been allocated in the variable.

