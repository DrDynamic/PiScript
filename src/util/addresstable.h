#pragma once


#include "../common.h"
#include "../table.h"
#include "VarArray.h"

typedef struct {
    Table addresses;
    VarArray props;
} AddressTable;


void initAddressTable(AddressTable* table);
void freeAddressTable(AddressTable* table);

uint32_t addresstableAdd(AddressTable* table, ObjString* name, Var props);
void addresstablePop(AddressTable* table);

bool addresstableIsEmpty(AddressTable* table);
bool addresstableGetAddress(AddressTable* table, ObjString* name, uint32_t* address);
bool addresstableGetName(AddressTable* table, uint32_t address, ObjString** name);
Var* addresstableGetLastProps(AddressTable* table);
Var* addresstableGetProps(AddressTable* table, uint32_t address);
