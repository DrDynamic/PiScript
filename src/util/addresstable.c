#include "addresstable.h"

void initAddressTable(AddressTable* table)
{
    initTable(&table->addresses);
    initVarArray(&table->props);
}

void freeAddressTable(AddressTable* table)
{
    freeTable(&table->addresses);
    freeVarArray(&table->props);

    initAddressTable(table);
}


uint32_t addresstableAdd(AddressTable* table, ObjString* name, Var props)
{
    uint32_t addr = table->props.count;
    if (addresstableGetAddress(table, name, &addr)) {
        tableSetUint32(&table->addresses, name, table->props.count);
        writeVarArray(&table->props,
            (Var) {
                .identifier = props.identifier,
                .depth = props.depth,
                .readonly = props.readonly,
                .shadowAddr = addr,
                .isCaptured = props.isCaptured,
            });
    } else {
        tableSetUint32(&table->addresses, name, table->props.count);
        writeVarArray(&table->props, props);
        addr = table->props.count - 1;
    }
    return addr;
}

void addresstablePop(AddressTable* table)
{
    Var* local = addresstableGetLastProps(table);
    if (local->shadowAddr != -1) {
        tableSetUint32(&table->addresses, local->identifier, local->shadowAddr);
    } else {
        tableDelete(&table->addresses, local->identifier);
    }

    table->props.count--;
}


bool addresstableIsEmpty(AddressTable* table)
{
    return table->props.count == 0;
}

inline bool addresstableGetAddress(AddressTable* table, ObjString* name, uint32_t* address)
{
    return tableGetUint32(&table->addresses, name, address);
}

inline ObjString* addresstableGetName(AddressTable* table, uint32_t address)
{
    return table->props.values[address].identifier;
}

inline Var* addresstableGetLastProps(AddressTable* table)
{
    return &table->props.values[table->props.count - 1];
}

inline Var* addresstableGetProps(AddressTable* table, uint32_t address)
{
    return &table->props.values[address];
}
