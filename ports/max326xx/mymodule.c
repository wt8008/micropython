#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"
//#include "portmodules.h"

#include <stdio.h>

// this is the actual C-structure for our new class object
typedef struct _mymodule_hello_obj_t
{
    mp_obj_base_t base;
    uint8_t hello_number;
} mymodule_hello_obj_t;


/**
 * Class local methods
 */
STATIC mp_obj_t mymodule_hello_increment(mp_obj_t self_in)
{
    mymodule_hello_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->hello_number += 1;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mymodule_hello_increment_obj,
                          mymodule_hello_increment);

// Table of class local members
STATIC const mp_rom_map_elem_t mymodule_hello_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_inc), MP_ROM_PTR(&mymodule_hello_increment_obj)},
};

STATIC MP_DEFINE_CONST_DICT(mymodule_hello_locals_dict,
                            mymodule_hello_locals_dict_table);

/** 
 * Standard class methods
 */
const mp_obj_type_t mymodule_helloObj_type;

mp_obj_t mymodule_hello_make_new(const mp_obj_type_t *type,
    size_t n_args,
    size_t n_kw,
    const mp_obj_t *args)
{
    mp_arg_check_num(n_args, n_kw, 1, 1, true);
    mymodule_hello_obj_t *self = m_new_obj(mymodule_hello_obj_t);
    self->base.type = &mymodule_helloObj_type;
    self->hello_number = mp_obj_get_int(args[0]);
    return MP_OBJ_FROM_PTR(self);
}

STATIC void mymodule_hello_print(const mp_print_t *print,
     mp_obj_t self_in,
     mp_print_kind_t kind)
{
    mymodule_hello_obj_t *self = MP_OBJ_TO_PTR(self_in);
    printf("Hello(%u)\n", self->hello_number);
}

/**
 * Class definition
 */
// create the class-object iself
const mp_obj_type_t mymodule_helloObj_type = {
    // "inherit" the type "type"
    {&mp_type_type},
    // give it a name
    .name = MP_QSTR_helloObj,
    // give it a print-function
    .print = mymodule_hello_print,
    // give it a constructor
    .make_new = mymodule_hello_make_new,
    // add the global members for the class
    .locals_dict = (mp_obj_dict_t *)&mymodule_hello_locals_dict,
};

/**
 * Module level functions 
 */
STATIC mp_obj_t mymodule_hello(mp_obj_t what)
{
    printf("Hello %s!\n", mp_obj_str_get_str(what));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mymodule_hello_obj, mymodule_hello);

/**
 * Module level definitions
 */

// Table of module global members
STATIC const mp_map_elem_t mymodule_globals_table[] = {
    {MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_mymodule)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_hello), (mp_obj_t)&mymodule_hello_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_helloObj), (mp_obj_t)&mymodule_helloObj_type},
};

STATIC MP_DEFINE_CONST_DICT(mp_module_mymodule_globals,
                            mymodule_globals_table);

// module definition
const mp_obj_module_t mp_module_mymodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&mp_module_mymodule_globals,
};


