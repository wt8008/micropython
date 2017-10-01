#include "py/obj.h"
#include "max3263x.h"
#include "pwrman_regs.h"

STATIC mp_obj_t machine_reset(void)
{
    MXC_PWRMAN->pwr_rst_ctrl |= MXC_F_PWRMAN_PWR_RST_CTRL_FIRMWARE_RESET;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_obj, machine_reset);

/** 
 * Module level definition
 */
// Table of module global members
STATIC const mp_map_elem_t machine_globals_table[] = {
    {MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_machine)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_reset), (mp_obj_t)&machine_reset_obj},
};

STATIC MP_DEFINE_CONST_DICT(mp_module_machine_globals, 
    machine_globals_table);

// module definition
const mp_obj_module_t mp_module_machine = {
     {&mp_type_module},
    .globals = (mp_obj_dict_t*)&mp_module_machine_globals,
};