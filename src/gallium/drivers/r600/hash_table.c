#include <string.h>
#include "hash_table.h"
#include "r600.h"
#include "r600_public.h"
#include "r600_shader.h"
#include "r600_resource.h"
#include "evergreend.h"
#include "r600_hw_context_priv.h"

struct r600_reg_hash_table hash_table_create(struct r600_context *ctx, int size)
{
  struct r600_reg_hash_table result;
  
  result.ctx = ctx;
  result.size = size;
  result.table = (struct r600_reg_hash*)malloc(size*sizeof(struct r600_reg_hash));
  
  memset(result.table, 0, size*sizeof(struct r600_reg_hash));
  
  return result;
}

static unsigned hash_table_rehash(unsigned hash)
{
  if (hash == 0)
  {
    hash = 4242427;
  }
  
  hash = (hash >> 1) ^ (-(hash & 1u) & 0xD0000001u); 
  hash = (hash >> 1) ^ (-(hash & 1u) & 0xD0000001u); 
  hash = (hash >> 1) ^ (-(hash & 1u) & 0xD0000001u); 
  hash = (hash >> 1) ^ (-(hash & 1u) & 0xD0000001u); 
  
  return hash;
}

static struct r600_reg_hash* hash_table_get_slot(struct r600_reg_hash_table table, unsigned index)
{
  unsigned hash = index;
  
  while (table.table[hash%table.size].reg_index != index && table.table[hash%table.size].reg_index != 0)
  {
    hash = hash_table_rehash(hash);
  }
  
  if (table.table[hash%table.size].reg_index == 0)
  {
    struct r600_reg_hash elem;
    
    elem.hash = hash;
    elem.reg_index = index;
    
    table.table[hash%table.size] = elem;
  }
  
  return &table.table[hash%table.size];
}

void hash_table_set_reg_reloc(struct r600_reg_hash_table table, unsigned index, unsigned value, unsigned mask, struct r600_resource *bo, enum radeon_bo_usage usage)
{
  struct r600_reg_hash* slot;
  
  slot = hash_table_get_slot(table, index);
  
  slot->value = (slot->value & ~mask) | (value & mask);
  slot->bo = bo;
  slot->usage = usage;  
}

void hash_table_set_reg(struct r600_reg_hash_table table, unsigned index, unsigned value, unsigned mask)
{
  struct r600_reg_hash* slot;
  
  slot = hash_table_get_slot(table, index);
  
  slot->value = (slot->value & ~mask) | (value & mask);
  slot->bo = NULL;
}

unsigned hash_table_get_reg(struct r600_reg_hash_table table, unsigned index)
{
  struct r600_reg_hash* slot;
  
  slot = hash_table_get_slot(table, index);
  
  return slot->value;
}

u32* evergreen_emit_raw_reloc(u32* dst, struct r600_context *ctx, struct r600_resource *bo, enum radeon_bo_usage usage)
{
  assert(bo);
  
  *dst = PKT3(PKT3_NOP, 0, 0);
  dst++;
  *dst = r600_context_bo_reloc(ctx, bo, usage);
  dst++;
  
  return dst;
}

void hash_table_serialize(struct r600_reg_hash_table table, u32* dst, int dst_size)
{
  int i;
  u32* limit = dst + dst_size-4;
  
  for (i = 0; i < table.size; i++)
  {
    if (table.table[i].reg_index)
    {
      assert(dst < limit);
      dst = evergreen_emit_raw_reg_set(dst, table.ctx, table.table[i].reg_index, 1);
      *dst = table.table[i].value;
      dst++;
      
      if (table.table[i].bo)
      {
        dst = evergreen_emit_raw_reloc(dst, table.ctx, table.table[i].bo, table.table[i].usage);
      }
    }
  }
}

u32* evergreen_emit_raw_reg_set(u32* dst, struct r600_context *ctx, unsigned index, int num)
{
  if (index >= EVERGREEN_CONFIG_REG_OFFSET && index < EVERGREEN_CONFIG_REG_END)
  {
    dst[0] = PKT3(PKT3_SET_CONFIG_REG, num, 0);
    dst[1] = (index - EVERGREEN_CONFIG_REG_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_CONTEXT_REG_OFFSET && index < EVERGREEN_CONTEXT_REG_END)
  { 
    dst[0] = PKT3(PKT3_SET_CONTEXT_REG, num, 0);
    dst[1] = (index - EVERGREEN_CONTEXT_REG_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_RESOURCE_OFFSET && index < EVERGREEN_RESOURCE_END)
  { 
    dst[0] = PKT3(PKT3_SET_RESOURCE, num, 0);
    dst[1] = (index - EVERGREEN_RESOURCE_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_SAMPLER_OFFSET && index < EVERGREEN_SAMPLER_END)
  { 
    dst[0] = PKT3(PKT3_SET_SAMPLER, num, 0);
    dst[1] = (index - EVERGREEN_SAMPLER_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_CTL_CONST_OFFSET && index < EVERGREEN_CTL_CONST_END)
  { 
    dst[0] = PKT3(PKT3_SET_CTL_CONST, num, 0);
    dst[1] = (index - EVERGREEN_CTL_CONST_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_LOOP_CONST_OFFSET && index < EVERGREEN_LOOP_CONST_END)
  { 
    dst[0] = PKT3(PKT3_SET_LOOP_CONST, num, 0);
    dst[1] = (index - EVERGREEN_LOOP_CONST_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_BOOL_CONST_OFFSET && index < EVERGREEN_BOOL_CONST_END)
  { 
    dst[0] = PKT3(PKT3_SET_BOOL_CONST, num, 0);
    dst[1] = (index - EVERGREEN_BOOL_CONST_OFFSET) >> 2;
  }
  else
  {
    dst[0] = PKT0(index, num-1);
    dst--;
  }
  
  dst += 2;
  
  return dst;
}

