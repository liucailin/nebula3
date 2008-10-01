//------------------------------------------------------------------------------
//  attributetable.cc
//  (C) 2006 Radon Labs GmbH
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "attr/attributetable.h"

namespace Attr
{
ImplementClass(Attr::AttributeTable, 'ATTT', Core::RefCounted);

using namespace Util;
using namespace Math;

//------------------------------------------------------------------------------
/**
*/
AttributeTable::AttributeTable() :
    rowPitch(0),
    numRows(0),
    allocatedRows(0),
    valueBuffer(0),
    rowNewBuffer(0),
    rowModifiedBuffer(0),
    rowDeletedBuffer(0),
    isModified(false),
    rowsModified(false),
    trackModifications(true),
    inBeginAddColumns(false),
    addColumnsRecordAsNewColumns(false),
    firstNewColumnIndex(InvalidIndex),
    columns(128, 128),
    readWriteColumnIndices(128, 128),
    newColumnIndices(128, 128),
    newRowIndices(128, 128),
    deletedRowIndices(128, 128),
    userData(128, 128)
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
AttributeTable::~AttributeTable()
{
    this->Delete();
}

//------------------------------------------------------------------------------
/**
    This method resets the object to the unmodified state, which means 
    the new-row and new-column index arrays are reset, and all modified bits
    are cleared.
*/
void
AttributeTable::ResetModifiedState()
{
    this->newColumnIndices.Clear();
    this->newRowIndices.Clear();
    this->deletedRowIndices.Clear();
    if (0 != this->rowModifiedBuffer)
    {
        Memory::Clear(this->rowModifiedBuffer, this->numRows);
    }
    if (0 != this->rowNewBuffer)
    {
        Memory::Clear(this->rowNewBuffer, this->numRows);
    }
    if (0 != this->rowDeletedBuffer)
    {
        Memory::Clear(this->rowDeletedBuffer, this->numRows);
    }
    this->isModified = false;
    this->rowsModified = false;
}

//------------------------------------------------------------------------------
/**
*/
void
AttributeTable::CopyString(IndexT colIndex, IndexT rowIndex, const Util::String& val)
{
    n_assert(this->GetColumnValueType(colIndex) == Attr::StringType);
    Util::String** valuePtr = (Util::String**) this->GetValuePtr(colIndex, rowIndex);
    if (0 == *valuePtr)
    {
        // allocate new object
        *valuePtr = n_new(Util::String(val));
    }
    else
    {
        // reuse existing object
        **valuePtr = val;
    }
}

//------------------------------------------------------------------------------
/**
*/
void
AttributeTable::DeleteString(IndexT colIndex, IndexT rowIndex)
{
    n_assert(this->GetColumnValueType(colIndex) == Attr::StringType);
    Util::String** valuePtr = (Util::String**) this->GetValuePtr(colIndex, rowIndex);
    if (0 != *valuePtr)
    {
        n_delete(*valuePtr);
        *valuePtr = 0;
    }
}

//------------------------------------------------------------------------------
/**
*/
void
AttributeTable::CopyGuid(IndexT colIndex, IndexT rowIndex, const Util::Guid& val)
{
    n_assert(this->GetColumnValueType(colIndex) == Attr::GuidType);
    Util::Guid** valuePtr = (Util::Guid**) this->GetValuePtr(colIndex, rowIndex);
    if (0 == *valuePtr)
    {
        // allocate new object
        *valuePtr = n_new(Util::Guid(val));
    }
    else
    {
        // reuse existing object
        **valuePtr = val;
    }
}

//------------------------------------------------------------------------------
/**
*/
void
AttributeTable::DeleteGuid(IndexT colIndex, IndexT rowIndex)
{
    n_assert(this->GetColumnValueType(colIndex) == Attr::GuidType);
    Util::Guid** valuePtr = (Util::Guid**) this->GetValuePtr(colIndex, rowIndex);
    if (0 != *valuePtr)
    {
        n_delete(*valuePtr);
        *valuePtr = 0;
    }
}

//------------------------------------------------------------------------------
/**
*/
void
AttributeTable::CopyBlob(IndexT colIndex, IndexT rowIndex, const Util::Blob& val)
{
    n_assert(this->GetColumnValueType(colIndex) == Attr::BlobType);
    Util::Blob** valuePtr = (Util::Blob**) this->GetValuePtr(colIndex, rowIndex);
    if (0 == *valuePtr)
    {
        // allocate new object
        *valuePtr = n_new(Util::Blob(val));
    }
    else
    {
        // reuse existing object
        **valuePtr = val;
    }
}

//------------------------------------------------------------------------------
/**
*/
void
AttributeTable::DeleteBlob(IndexT colIndex, IndexT rowIndex)
{
    n_assert(this->GetColumnValueType(colIndex) == Attr::BlobType);
    Util::Blob** valuePtr = (Util::Blob**) this->GetValuePtr(colIndex, rowIndex);
    if (0 != *valuePtr)
    {
        n_delete(*valuePtr);
        *valuePtr = 0;
    }
}

//------------------------------------------------------------------------------
/**
    This frees all allocated data associated with the table.
*/
void
AttributeTable::Delete()
{
    // for each column with externally allocated data...
    IndexT rowIndex;
    IndexT colIndex;
    for (colIndex = 0; colIndex < this->columns.Size(); colIndex++)
    {
        switch (this->GetColumnValueType(colIndex))
        {
            case Attr::StringType:
                for (rowIndex = 0; rowIndex < this->numRows; rowIndex++)
                {
                    this->DeleteString(colIndex, rowIndex);
                }
                break;

            case Attr::GuidType:
                for (rowIndex = 0; rowIndex < this->numRows; rowIndex++)
                {
                    this->DeleteGuid(colIndex, rowIndex);
                }
                break;

            case Attr::BlobType:
                for (rowIndex = 0; rowIndex < this->numRows; rowIndex++)
                {
                    this->DeleteBlob(colIndex, rowIndex);
                }
                break;
        }
    }
    if (0 != this->valueBuffer)
    {
        Memory::Free(Memory::DefaultHeap, this->valueBuffer);
        this->valueBuffer = 0;
    }
    if (0 != this->rowModifiedBuffer)
    {
        Memory::Free(Memory::SmallBlockHeap, this->rowModifiedBuffer);
        this->rowModifiedBuffer = 0;
    }
    if (0 != this->rowDeletedBuffer)
    {
        Memory::Free(Memory::SmallBlockHeap, this->rowDeletedBuffer);
        this->rowDeletedBuffer = 0;
    }
    if (0 != this->rowNewBuffer)
    {
        Memory::Free(Memory::SmallBlockHeap, this->rowNewBuffer);
        this->rowNewBuffer = 0;
    }
    this->numRows = 0;
    this->allocatedRows = 0;
    this->isModified = false;
    this->rowsModified = false;
    this->userData.Clear();
}

//------------------------------------------------------------------------------
/**
    Clear all rows data and reset the number of rows.
*/
void
AttributeTable::Clear()
{
    this->Delete();
}

//------------------------------------------------------------------------------
/**
    Reallocate the value and modified row buffer, and copy contents over into 
    the new buffer.
*/
void
AttributeTable::Reallocate(SizeT newPitch, SizeT newAllocRows)
{
    n_assert(newAllocRows >= this->numRows);
    n_assert(newPitch >= this->rowPitch);

    // allocate new value buffer
    this->allocatedRows = newAllocRows;
    SizeT newValueBufferSize = newPitch * newAllocRows;
    void* newValueBuffer = Memory::Alloc(Memory::DefaultHeap, newValueBufferSize);
    Memory::Clear(newValueBuffer, newValueBufferSize);

    // copy over value buffer contents
    if (0 != this->valueBuffer)
    {
        IndexT rowIndex;
        char* fromPtr = (char*) this->valueBuffer;
        char* toPtr = (char*) newValueBuffer;
        if (newPitch == this->rowPitch)
        {
            // same pitch, copy one big block
            Memory::Copy(fromPtr, toPtr, this->rowPitch * this->numRows);
        }
        else
        {
            // different pitch, several copies needed
            for (rowIndex = 0; rowIndex < this->numRows; rowIndex++)
            {
                Memory::Copy(fromPtr, toPtr, this->rowPitch);
                fromPtr += this->rowPitch;
                toPtr += newPitch;
            }
        }

        // free old array 
        Memory::Free(Memory::DefaultHeap, this->valueBuffer);
        this->valueBuffer = 0;
    }
    
    // handle modified row buffer
    uchar* newRowModifiedBuffer = (uchar*) Memory::Alloc(Memory::SmallBlockHeap, newAllocRows);
    Memory::Clear(newRowModifiedBuffer, newAllocRows);
    if (0 != this->rowModifiedBuffer)
    {
        Memory::Copy(this->rowModifiedBuffer, newRowModifiedBuffer, this->numRows);
        Memory::Free(Memory::SmallBlockHeap, this->rowModifiedBuffer);
        this->rowModifiedBuffer = 0;
    }

    // handle deleted row buffer
    uchar* newRowDeletedBuffer = (uchar*) Memory::Alloc(Memory::SmallBlockHeap, newAllocRows);
    Memory::Clear(newRowDeletedBuffer, newAllocRows);
    if (0 != this->rowDeletedBuffer)
    {
        Memory::Copy(this->rowDeletedBuffer, newRowDeletedBuffer, this->numRows);
        Memory::Free(Memory::SmallBlockHeap, this->rowDeletedBuffer);
        this->rowDeletedBuffer = 0;
    }

    // handle new row buffer
    uchar* newRowNewBuffer = (uchar*) Memory::Alloc(Memory::SmallBlockHeap, newAllocRows);
    Memory::Clear(newRowNewBuffer, newAllocRows);
    if (0 != this->rowNewBuffer)
    {
        Memory::Copy(this->rowNewBuffer, newRowNewBuffer, this->numRows);
        Memory::Free(Memory::SmallBlockHeap, this->rowNewBuffer);
        this->rowNewBuffer = 0;
    }

    // set new members
    this->valueBuffer = newValueBuffer;
    this->rowModifiedBuffer = newRowModifiedBuffer;
    this->rowDeletedBuffer = newRowDeletedBuffer;
    this->rowNewBuffer = newRowNewBuffer;
    this->rowPitch = newPitch;
}

//------------------------------------------------------------------------------
/**
    This returns the cell size for a value type. The minimum size is 4
    (even for Bools), for data alignment reasons. Strings are stored as
    char pointers, the rest takes as much memory as needed.
*/
SizeT
AttributeTable::GetValueTypeSize(ValueType type) const
{
    switch (type)
    {
        case IntType:       return sizeof(int);
        case BoolType:      return sizeof(int);     // not a bug!
        case FloatType:     return sizeof(float);
        case Float4Type:    return sizeof(float4);
        case Matrix44Type:  return sizeof(matrix44);
        case StringType:    return sizeof(Util::String*);
        case GuidType:      return sizeof(Util::Guid*);
        case BlobType:      return sizeof(Util::Blob*);
    }
    return 0;
}

//------------------------------------------------------------------------------
/**
    This updates the byte offsets for the current column configuration.
*/
SizeT
AttributeTable::UpdateColumnOffsets()
{
    // update column offsets
    IndexT curOffset = 0;
    IndexT i;
    for (i = 0; i < this->columns.Size(); i++)
    {
        this->columns[i].byteOffset = curOffset;
        curOffset += this->GetValueTypeSize(columns[i].attrId.GetValueType());
    }

    // align the offset to 4 bytes and use that as row pitch
    SizeT newPitch = ((curOffset + 3) / 4) * 4;
    return newPitch;
}

//------------------------------------------------------------------------------
/**
    Begin adding columns. Columns can be added at any time, but it will
    be much more efficient when called between BeginAddColumns() and
    EndAddColumns(), since this will save a lot of re-allocations.
*/
void
AttributeTable::BeginAddColumns(bool recordAsNewColumns)
{
    n_assert(!this->inBeginAddColumns);
    this->inBeginAddColumns = true;
    this->addColumnsRecordAsNewColumns = recordAsNewColumns;
    this->firstNewColumnIndex = this->columns.Size();
}

//------------------------------------------------------------------------------
/**
    Internal helper method to add a new column. This is called
    by EndAddColumns() and AddColumn().
*/
void
AttributeTable::InternalAddColumnHelper(const AttrId& id, bool recordAsNewColumn)
{
    // make sure the column doesn't exist yet
    n_assert(!this->indexMap.Contains(id));

    // add a new column info structure
    ColumnInfo newColumnInfo;
    newColumnInfo.attrId = id;
    newColumnInfo.byteOffset = 0;
    this->columns.Append(newColumnInfo);
    if (this->trackModifications)
    {
    if (recordAsNewColumn)
    {
        this->newColumnIndices.Append(this->columns.Size() - 1);
    }
    this->isModified = true;
    }

    // keep track of read/write columns
    if (id.GetAccessMode() == Attr::ReadWrite)
    {
        this->readWriteColumnIndices.Append(this->columns.Size() - 1);
    }

    // add to lookup table for fast lookup by attribute id
    this->indexMap.Add(id, this->columns.Size() - 1);
}

//------------------------------------------------------------------------------
/**
    End adding columns. This will do the actual work.
*/
void
AttributeTable::EndAddColumns()
{
    n_assert(this->inBeginAddColumns);
    this->inBeginAddColumns = false;

    // only do something if new columns have actually been added
    if (this->firstNewColumnIndex < this->columns.Size())
    {
        // recompute column byte offset and pitch values and re-allocate data table
        SizeT newPitch = this->UpdateColumnOffsets();
        if (this->numRows > 0)
        {
            this->Reallocate(newPitch, this->numRows);
        }
        else
        {
            this->rowPitch = newPitch;
        }

        // set default values on new columns
        IndexT colIndex;
        for (colIndex = this->firstNewColumnIndex; colIndex < this->columns.Size(); colIndex++)
        {
            // fill column with default values
            this->SetColumnToDefaultValues(colIndex);
        }
    }
}

//------------------------------------------------------------------------------
/**
    Add a column to the attribute table. If the attribute table already
    contains data, this will reallocate the existing data buffer. The name,
    data type, access mode, etc of the column is all defined by the 
    given attribute id. The new column will be filled with the
    attribute id's default value.
*/
void
AttributeTable::AddColumn(const AttrId& id, bool recordAsNewColumn)
{
    // call internal add column method
    this->InternalAddColumnHelper(id, recordAsNewColumn);

    // if not in begin/end, need to update data buffer immediately
    if (!this->inBeginAddColumns)
    {
    // recompute column byte offset and pitch values
    SizeT newPitch = this->UpdateColumnOffsets();

	    // if necessary, re-allocate value buffer
	    if (this->numRows > 0)
	    {
	        this->Reallocate(newPitch, this->numRows);
	    }
	    else
	    {
	        this->rowPitch = newPitch;
	    }

        // fill column with default values
        IndexT colIndex = this->columns.Size() - 1;
        this->SetColumnToDefaultValues(colIndex);
    }
}

//------------------------------------------------------------------------------
/**
    This marks a row for deletion. Note that the row will only be marked
    for deletion, deleted row indices are returned with the 
    GetDeletedRowIndices() call. The row will never be physically removed
    from memory!
*/
void
AttributeTable::DeleteRow(IndexT rowIndex)
{
    n_assert(rowIndex < this->numRows);
    n_assert(0 != this->rowDeletedBuffer);
    n_assert(!this->IsRowDeleted(rowIndex));
    if (this->trackModifications)
    {
    this->deletedRowIndices.Append(rowIndex);
    this->rowDeletedBuffer[rowIndex] = 1;
    this->isModified = true;
}
    this->userData[rowIndex] = 0;
}

//------------------------------------------------------------------------------
/**
    Marks all rows in the table as deleted.
*/
void
AttributeTable::DeleteAllRows()
{
    IndexT rowIndex;
    for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
    {
        this->DeleteRow(rowIndex);
    }
}

//------------------------------------------------------------------------------
/**
    This creates a new row as a copy of an existing row. Returns the
    index of the new row. NOTE: the user data will be initialized to
    0 for the new row!
*/
IndexT
AttributeTable::CopyRow(IndexT fromRowIndex)
{
    n_assert(fromRowIndex < this->numRows);
    IndexT toRowIndex = this->AddRow();
    IndexT colIndex;
    for (colIndex = 0; colIndex < this->columns.Size(); colIndex++)
    {
        switch (this->GetColumnValueType(colIndex))
        {
            case IntType:
                this->SetInt(colIndex, toRowIndex, this->GetInt(colIndex, fromRowIndex));
                break;

            case FloatType:
                this->SetFloat(colIndex, toRowIndex, this->GetFloat(colIndex, fromRowIndex));
                break;

            case BoolType:
                this->SetBool(colIndex, toRowIndex, this->GetBool(colIndex, fromRowIndex));
                break;

            case Float4Type:
                this->SetFloat4(colIndex, toRowIndex, this->GetFloat4(colIndex, fromRowIndex));
                break;

            case StringType:
                this->SetString(colIndex, toRowIndex, this->GetString(colIndex, fromRowIndex));
                break;

            case Matrix44Type:
                this->SetMatrix44(colIndex, toRowIndex, this->GetMatrix44(colIndex, fromRowIndex));
                break;

            case BlobType:
                this->SetBlob(colIndex, toRowIndex, this->GetBlob(colIndex, fromRowIndex));
                break;

            case GuidType:
                this->SetGuid(colIndex, toRowIndex, this->GetGuid(colIndex, fromRowIndex));
                break;

            default:
                n_error("AttributeTable::CopyRow(): unsupported attribute type!");
                break;
        }
    }
    return toRowIndex;
}

//------------------------------------------------------------------------------
/**
    Create a new row as a copy of a row in another value table. The
    layouts of the value tables must not match, since only matching
    columns will be considered. NOTE: the user data will be initialised
    to 0 for the new row.
*/
IndexT
AttributeTable::CopyExtRow(AttributeTable* other, IndexT otherRowIndex, bool createMissingRows)
{
    n_assert(0 != other);
    n_assert(otherRowIndex < other->GetNumRows());
    IndexT myRowIndex = this->AddRow();
    IndexT otherColIndex;
    for (otherColIndex = 0; otherColIndex < other->GetNumColumns(); otherColIndex++)
    {
        const AttrId& attrId = other->GetColumnId(otherColIndex);

        if (createMissingRows && !this->HasColumn(attrId))
        {
            this->AddColumn(attrId);
        }

        if (this->HasColumn(attrId))
        {
            switch (other->GetColumnValueType(otherColIndex))
            {
                case IntType:
                    this->SetInt(attrId, myRowIndex, other->GetInt(otherColIndex, otherRowIndex));
                    break;

                case FloatType:
                    this->SetFloat(attrId, myRowIndex, other->GetFloat(otherColIndex, otherRowIndex));
                    break;

                case BoolType:
                    this->SetBool(attrId, myRowIndex, other->GetBool(otherColIndex, otherRowIndex));
                    break;

                case Float4Type:
                    this->SetFloat4(attrId, myRowIndex, other->GetFloat4(otherColIndex, otherRowIndex));
                    break;

                case StringType:
                    this->SetString(attrId, myRowIndex, other->GetString(otherColIndex, otherRowIndex));
                    break;

                case Matrix44Type:
                    this->SetMatrix44(attrId, myRowIndex, other->GetMatrix44(otherColIndex, otherRowIndex));
                    break;

                case BlobType:
                    this->SetBlob(attrId, myRowIndex, other->GetBlob(otherColIndex, otherRowIndex));
                    break;

                case GuidType:
                    this->SetGuid(attrId, myRowIndex, other->GetGuid(otherColIndex, otherRowIndex));
                    break;

                default:
                    n_error("AttributeTable::CopyExtRow(): unsupported attribute type!");
                    break;
            }
        }
    }
    return myRowIndex;
}

//------------------------------------------------------------------------------
/**
    Reserve N more rows beforehand to reduce re-allocation overhead during 
    AddRow().
*/
void
AttributeTable::ReserveRows(SizeT num)
{
    n_assert(num > 0);
    this->Reallocate(this->rowPitch, this->allocatedRows + num);
}

//------------------------------------------------------------------------------
/**
    Adds an empty row at the end of the value buffer. The row will be
    marked invalid until the first value is set in the row. This will
    re-allocate the existing value buffer. If you know beforehand how
    many rows will exist in the table it is more efficient to use
    one SetNumRows(N) instead of N times AddRow()! The method returns
    the index of the newly added row. The row will be filled with
    the row attribute's default values.
*/
IndexT
AttributeTable::AddRow()
{
    // check if we need to grow the buffer
    if (this->numRows >= this->allocatedRows)
    {
        int newNumRows = this->allocatedRows + this->allocatedRows;
        if (0 == newNumRows)
        {
            newNumRows = 10;
        }
        this->Reallocate(this->rowPitch, newNumRows);
    }
    if (this->trackModifications)
    {
        this->newRowIndices.Append(this->numRows);
        this->rowNewBuffer[this->numRows] = 1;
        this->isModified = true;
    }
    this->userData.Append(0);

    // fill with default values
    IndexT rowIndex = this->numRows++;
    this->SetRowToDefaultValues(rowIndex);
    return rowIndex;
}

//------------------------------------------------------------------------------
/**
    Clears the new row flags. After this, new rows are treated just like
    updated rows, which may be useful for some database operations (when
    an UPDATE is wanted instead of an INSERT).
*/
void
AttributeTable::ClearNewRowFlags()
{
    this->newRowIndices.Clear();
    Memory::Clear(this->rowNewBuffer, this->numRows);
}

//------------------------------------------------------------------------------
/**
    Clears the deleted row flags.
*/
void
AttributeTable::ClearDeletedRowsFlags()
{
    this->deletedRowIndices.Clear();
}

//------------------------------------------------------------------------------
/**
    Finds a row index by multiple attribute values. This method can be slow since
    it may search linearly (and vertically) through the table. Also, for one
    attribute this method is slower then InternalFindRowIndicesByAttr()!

    FIXME: keep row indices for indexed rows in nDictionaries?
*/
Util::Array<IndexT>
AttributeTable::InternalFindRowIndicesByAttrs(const Util::Array<Attribute>& attrs, bool firstMatchOnly) const
{
    Util::Array<IndexT> result;

    // create a table of column indices for each attribute
    Util::FixedArray<IndexT> attrColIndices(attrs.Size());
    IndexT attrIndex;
    SizeT numAttrs = attrs.Size();
    for (attrIndex = 0; attrIndex < numAttrs; attrIndex++)
    {
        attrColIndices[attrIndex] = this->GetColumnIndex(attrs[attrIndex].GetAttrId());
    }
    // for each row...
    IndexT rowIndex;
    for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
    {
        if (!this->IsRowDeleted(rowIndex))
        {
            // for each attribute...
            bool isEqual = true;
            IndexT attrIndex;
            for (attrIndex = 0; isEqual && (attrIndex < numAttrs); attrIndex++)
            {
                IndexT colIndex = attrColIndices[attrIndex];
                const Attribute& attr = attrs[attrIndex];
                switch (attr.GetValueType())
                {
                    case IntType:
                        if (attr.GetInt() != this->GetInt(colIndex, rowIndex))
                        {
                            isEqual = false;
                            break;
                        }
                        break;

                    case FloatType:
                        if (attr.GetFloat() != this->GetFloat(colIndex, rowIndex))
                        {
                            isEqual = false;
                            break;
                        }
                        break;

                    case BoolType:
                        if (attr.GetBool() != this->GetBool(colIndex, rowIndex))
                        {
                            isEqual = false;
                            break;
                        }
                        break;

                    case Float4Type:
                        if (attr.GetFloat4() != this->GetFloat4(colIndex, rowIndex))
                        {
                            isEqual = false;
                            break;
                        }
                        break;

                    case StringType:
                        if (attr.GetString() != this->GetString(colIndex, rowIndex))
                        {
                            isEqual = false;
                            break;
                        }
                        break;

                    case BlobType:
                        if (attr.GetBlob() != this->GetBlob(colIndex, rowIndex))
                        {
                            isEqual = false;
                            break;
                        }
                        break;

                    case GuidType:
                        if (attr.GetGuid() != this->GetGuid(colIndex, rowIndex))
                        {
                            isEqual = false;
                            break;
                        }
                }
            }
            if (isEqual)
            {
                result.Append(rowIndex);
                if (firstMatchOnly)
                {
                    return result;
                }
            }
        }
    }
    return result;
}

//------------------------------------------------------------------------------
/**
    Finds a row index by single attribute value. This method can be slow since
    it may search linearly (and vertically) through the table. 

    FIXME: keep row indices for indexed rows in nDictionaries?
*/
Util::Array<IndexT>
AttributeTable::InternalFindRowIndicesByAttr(const Attribute& attr, bool firstMatchOnly) const
{
    Util::Array<IndexT> result;
    IndexT colIndex = this->GetColumnIndex(attr.GetAttrId());
    ValueType colType = this->GetColumnValueType(colIndex);
    n_assert(colType == attr.GetValueType());
    IndexT rowIndex;
    switch (colType)
    {
        case IntType:
            for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
            {
                if ((!this->IsRowDeleted(rowIndex)) && (attr.GetInt() == this->GetInt(colIndex, rowIndex)))
                {
                    result.Append(rowIndex);
                    if (firstMatchOnly) return result;
                }
            }
            break;

        case FloatType:
            for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
            {
                if ((!this->IsRowDeleted(rowIndex)) && (attr.GetFloat() == this->GetFloat(colIndex, rowIndex)))
                {
                    result.Append(rowIndex);
                    if (firstMatchOnly) return result;
                }
            }
            break;

        case BoolType:
            for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
            {
                if ((!this->IsRowDeleted(rowIndex)) && (attr.GetBool() == this->GetBool(colIndex, rowIndex)))
                {
                    result.Append(rowIndex);
                    if (firstMatchOnly) return result;
                }
            }
            break;

        case Float4Type:
            for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
            {
                if ((!this->IsRowDeleted(rowIndex)) && (attr.GetFloat4() == this->GetFloat4(colIndex, rowIndex)))
                {
                    result.Append(rowIndex);
                    if (firstMatchOnly) return result;
                }
            }
            break;

        case StringType:
            for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
            {
                if ((!this->IsRowDeleted(rowIndex)) && (attr.GetString() == this->GetString(colIndex, rowIndex)))
                {
                    result.Append(rowIndex);
                    if (firstMatchOnly) return result;
                }
            }
            break;

        case BlobType:
            for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
            {
                if ((!this->IsRowDeleted(rowIndex)) && (attr.GetBlob() == this->GetBlob(colIndex, rowIndex)))
                {
                    result.Append(rowIndex);
                    if (firstMatchOnly) return result;
                }
            }
            break;

        case GuidType:
            for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
            {
                if ((!this->IsRowDeleted(rowIndex)) && (attr.GetGuid() == this->GetGuid(colIndex, rowIndex)))
                {
                    result.Append(rowIndex);
                    if (firstMatchOnly) return result;
                }
            }
            break;
    }
    return result;
}

//------------------------------------------------------------------------------
/**
    Finds multiple row indices by matching attribute. This method can be slow since
    it may search linearly (and vertically) through the table.
*/
Util::Array<IndexT>
AttributeTable::FindRowIndicesByAttr(const Attr::Attribute& attr, bool firstMatchOnly) const
{
    return this->InternalFindRowIndicesByAttr(attr, firstMatchOnly);
}

//------------------------------------------------------------------------------
/**
    Finds multiple row indices by multiple matching attribute. This method can be slow since
    it may search linearly (and vertically) through the table.
*/
Util::Array<IndexT>
AttributeTable::FindRowIndicesByAttrs(const Util::Array<Attr::Attribute>& attrs, bool firstMatchOnly) const
{
    return this->InternalFindRowIndicesByAttrs(attrs, firstMatchOnly);
}

//------------------------------------------------------------------------------
/**
    Finds single row index by matching attribute. This method can be slow since
    it may search linearly (and vertically) through the table.
*/
IndexT
AttributeTable::FindRowIndexByAttr(const Attr::Attribute& attr) const
{
    Util::Array<IndexT> rowIndices = this->InternalFindRowIndicesByAttr(attr, true);
    if (rowIndices.Size() == 1)
    {
        return rowIndices[0];
    }
    else
    {
        return InvalidIndex;
    }
}

//------------------------------------------------------------------------------
/**
    Finds single row index by multiple matching attributes. This method can be slow since
    it may search linearly (and vertically) through the table.
*/
IndexT
AttributeTable::FindRowIndexByAttrs(const Util::Array<Attr::Attribute>& attrs) const
{
    Util::Array<IndexT> rowIndices = this->InternalFindRowIndicesByAttrs(attrs, true);
    if (rowIndices.Size() == 1)
    {
        return rowIndices[0];
    }
    else
    {
        return InvalidIndex;
    }
}

//------------------------------------------------------------------------------
/**
*/
void
AttributeTable::SetRowUserData(IndexT rowIndex, void* p)
{
    this->userData[rowIndex] = p;
}

//------------------------------------------------------------------------------
/**
*/
void*
AttributeTable::GetRowUserData(IndexT rowIndex) const
{
    return this->userData[rowIndex];
}

//------------------------------------------------------------------------------
/**
    This sets all attributes in a row to its default values.
*/
void
AttributeTable::SetRowToDefaultValues(IndexT rowIndex)
{
    IndexT colIndex;
    for (colIndex = 0; colIndex < this->GetNumColumns(); colIndex++)
    {
        const AttrId& colAttrId = this->GetColumnId(colIndex);
        switch (colAttrId.GetValueType())
        {
            case IntType:       this->SetInt(colIndex, rowIndex, colAttrId.GetIntDefValue()); break;
            case FloatType:     this->SetFloat(colIndex, rowIndex, colAttrId.GetFloatDefValue()); break;
            case BoolType:      this->SetBool(colIndex, rowIndex, colAttrId.GetBoolDefValue()); break;
            case Float4Type:    this->SetFloat4(colIndex, rowIndex, colAttrId.GetFloat4DefValue()); break;
            case StringType:    this->SetString(colIndex, rowIndex, colAttrId.GetStringDefValue()); break;
            case Matrix44Type:  this->SetMatrix44(colIndex, rowIndex, colAttrId.GetMatrix44DefValue()); break;
            case BlobType:      this->SetBlob(colIndex, rowIndex, colAttrId.GetBlobDefValue()); break;
            case GuidType:      this->SetGuid(colIndex, rowIndex, colAttrId.GetGuidDefValue()); break;
            default:
                n_error("Invalid column attribute type!");
                break;
        }
    }
}

//------------------------------------------------------------------------------
/**
    This sets all attributes in a column to its default values.
*/
void
AttributeTable::SetColumnToDefaultValues(IndexT colIndex)
{
    const AttrId& colAttrId = this->GetColumnId(colIndex);
    IndexT rowIndex;
    switch (colAttrId.GetValueType())
    {
        case IntType:
            {
                int def = colAttrId.GetIntDefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetInt(colIndex, rowIndex, def);
                }
            }
            break;

        case FloatType:
            {
                float def = colAttrId.GetFloatDefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetFloat(colIndex, rowIndex, def);
                }
            }
            break;

        case BoolType:
            {
                bool def = colAttrId.GetBoolDefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetBool(colIndex, rowIndex, def);
                }
            }
            break;

        case Float4Type:
            {
                const float4& def = colAttrId.GetFloat4DefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetFloat4(colIndex, rowIndex, def);
                }
            }
            break;

        case Matrix44Type:
            {
                const matrix44& def = colAttrId.GetMatrix44DefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetMatrix44(colIndex, rowIndex, def);
                }
            }
            break;

        case StringType:
            {
                const Util::String& def = colAttrId.GetStringDefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetString(colIndex, rowIndex, def);
                }
            }
            break;

        case BlobType:
            {
                const Util::Blob& def = colAttrId.GetBlobDefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetBlob(colIndex, rowIndex, def);
                }
            }
            break;

        case GuidType:
            {
                const Util::Guid& def = colAttrId.GetGuidDefValue();
                for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
                {
                    this->SetGuid(colIndex, rowIndex, def);
                }
            }
            break;

            default:
                n_error("Invalid column attribute type!");
                break;
    }
}

//------------------------------------------------------------------------------
/**
    Set a generic attribute, this is a slow method!
*/
void
AttributeTable::SetAttr(const Attr::Attribute& attr, IndexT rowIndex)
{
    IndexT colIndex = this->GetColumnIndex(attr.GetAttrId());
    switch (attr.GetValueType())
    {
        case IntType:       this->SetInt(colIndex, rowIndex, attr.GetInt()); break;
        case FloatType:     this->SetFloat(colIndex, rowIndex, attr.GetFloat()); break;
        case BoolType:      this->SetBool(colIndex, rowIndex, attr.GetBool()); break;
        case Float4Type:   this->SetFloat4(colIndex, rowIndex, attr.GetFloat4()); break;
        case StringType:    this->SetString(colIndex, rowIndex, attr.GetString()); break;
        case Matrix44Type:  this->SetMatrix44(colIndex, rowIndex, attr.GetMatrix44()); break;
        case BlobType:      this->SetBlob(colIndex, rowIndex, attr.GetBlob()); break;
        case GuidType:      this->SetGuid(colIndex, rowIndex, attr.GetGuid()); break;
        default:
            n_error("Invalid value type!");
            break;
    }
}

//------------------------------------------------------------------------------
/**
    Returns an array of all modified rows, but excludes rows marked as new.
*/
Util::Array<IndexT>
AttributeTable::GetModifiedRowsExcludeNewAndDeletedRows() const
{
    Util::Array<IndexT> result;
    IndexT rowIndex;
    for (rowIndex = 0; rowIndex < this->numRows; rowIndex++)
    {
        if ((this->rowModifiedBuffer[rowIndex] == 1) &&
            (this->rowNewBuffer[rowIndex] == 0) &&
            (this->rowDeletedBuffer[rowIndex] == 0))
        {
            result.Append(rowIndex);
        }
    }
    return result;
}   

//------------------------------------------------------------------------------
/**
    Print contents of the table for debugging.
*/
void
AttributeTable::PrintDebug()
{
    // print column titles, types and readwrite state
    IndexT colIndex;
    for (colIndex = 0; colIndex < this->GetNumColumns(); colIndex++)
    {
        n_printf("%s(%s,%s)\t", 
            this->GetColumnName(colIndex).AsCharPtr(),
            Attribute::ValueTypeToString(this->GetColumnValueType(colIndex)).AsCharPtr(),
            this->GetColumnAccessMode(colIndex) == ReadOnly ? "ro" : "rw");
    }
    n_printf("\n----\n");
    
    // print rows
    IndexT rowIndex;
    for (rowIndex = 0; rowIndex < this->GetNumRows(); rowIndex++)
    {
        for (colIndex = 0; colIndex < this->GetNumColumns(); colIndex++)
        {
            Util::String str = "(invalid)";
            switch (this->GetColumnValueType(colIndex))
            {
                case IntType:       str = Util::String::FromInt(this->GetInt(colIndex, rowIndex)); break;
                case FloatType:     str = Util::String::FromFloat(this->GetFloat(colIndex, rowIndex)); break;
                case BoolType:      str = Util::String::FromBool(this->GetBool(colIndex, rowIndex)); break;
                case Float4Type:   str = Util::String::FromFloat4(this->GetFloat4(colIndex, rowIndex)); break;
                case StringType:    str = this->GetString(colIndex, rowIndex); break;
                case Matrix44Type:  str = Util::String::FromMatrix44(this->GetMatrix44(colIndex, rowIndex)); break;
                case BlobType:      str = "(blob)"; break;
                case GuidType:      str = this->GetGuid(colIndex, rowIndex).AsString(); break;
            }
            n_printf("%s\t", str.AsCharPtr());
        }
        n_printf("\n");
    }
}

} // namespace Attr
