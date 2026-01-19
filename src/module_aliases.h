#ifndef MODULE_ALIASES_H
#define MODULE_ALIASES_H

// Shared module alias registry
void register_module_alias_global(const char* alias, const char* module);
const char* resolve_module_alias_global(const char* name);

#endif
