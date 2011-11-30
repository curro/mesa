#ifndef HASH_TABLE_H
#define HASH_TABLE_H
#include "r600.h"

struct r600_reg_hash
{
  unsigned hash;
  
  unsigned reg_index;
  unsigned value;
  struct r600_resource *bo;
  enum radeon_bo_usage usage;
  int bo_flags;
};

struct r600_reg_hash_table
{
  int size;
  struct r600_context *ctx;
  struct r600_reg_hash* table;
};

struct r600_reg_hash_table hash_table_create(struct r600_context *ctx, int size);
void hash_table_set_reg_reloc(struct r600_reg_hash_table table, unsigned index, unsigned value, unsigned mask, struct r600_resource *bo, enum radeon_bo_usage usage);
void hash_table_set_reg(struct r600_reg_hash_table table, unsigned index, unsigned value, unsigned mask);
unsigned hash_table_get_reg(struct r600_reg_hash_table table, unsigned index);
void hash_table_serialize(struct r600_reg_hash_table table, u32* dst, int dst_size);

u32* evergreen_emit_raw_reg_set(u32* dst, struct r600_context *ctx, unsigned index, int num);
u32* evergreen_emit_raw_reloc(u32* dst, struct r600_context *ctx, struct r600_resource *bo, enum radeon_bo_usage usage);

#endif
