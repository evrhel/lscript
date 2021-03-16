#if !defined(MATH_H)
#define MATH_H

#include "types.h"
#include "vm.h"

/*
Handles the add command

@param env A pointer to a valid env_t structure
@param argLoc A pointer to the current execution location

@return 1 if the add was a success or 0 if an exception was thrown
*/
int vmm_add(env_t *env, byte_t **argLoc);

/*
Handles the sub command

@param env A pointer to a valid env_t structure
@param argLoc A pointer to the current execution location

@return 1 if the add was a success or 0 if an exception was thrown
*/
int vmm_sub(env_t *env, byte_t **argLoc);

/*
Handles the mul command

@param env A pointer to a valid env_t structure
@param argLoc A pointer to the current execution location

@return 1 if the add was a success or 0 if an exception was thrown
*/
int vmm_mul(env_t *env, byte_t **argLoc);

/*
Handles the div command

@param env A pointer to a valid env_t structure
@param argLoc A pointer to the current execution location

@return 1 if the add was a success or 0 if an exception was thrown
*/
int vmm_div(env_t *env, byte_t **argLoc);

/*
Handles the mod command

@param env A pointer to a valid env_t structure
@param argLoc A pointer to the current execution location

@return 1 if the add was a success or 0 if an exception was thrown
*/
int vmm_mod(env_t *env, byte_t **argLoc);

int vmm_neg(env_t *env, byte_t **argLoc);

int vmm_and(env_t *env, byte_t **argLoc);

int vmm_or(env_t *env, byte_t **argLoc);

int vmm_xor(env_t *env, byte_t **argLoc);

int vmm_lsh(env_t *env, byte_t **argLoc);

int vmm_rsh(env_t *env, byte_t **argLoc);

int vmm_not(env_t *env, byte_t **argLoc);

#endif