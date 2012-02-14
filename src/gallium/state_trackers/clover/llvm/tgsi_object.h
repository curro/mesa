
enum {
  TGSI_SECTION_SYMTAB,
  TGSI_SECTION_TEXT,
  TGSI_SECTION_CONSTANT,
  TGSI_SECTION_GLOBAL,
  TGSI_SECTION_LOCAL,
  TGSI_SECTION_PRIVATE
};

enum {
  TGSI_ARGUMENT_INLINE,
  TGSI_ARGUMENT_CONSTANT,
  TGSI_ARGUMENT_GLOBAL,
  TGSI_ARGUMENT_LOCAL,
  TGSI_ARGUMENT_RDIMAGE2D,
  TGSI_ARGUMENT_WRIMAGE2D,
  TGSI_ARGUMENT_RDIMAGE3D,
  TGSI_ARGUMENT_WRIMAGE3D,
  TGSI_ARGUMENT_SAMPLER
};

struct tgsi_symbol {
  uint32_t resource_id;
  uint32_t offset;
  uint32_t args_sz;
  uint32_t name_sz;
};

struct tgsi_argument {
  uint32_t kind;
  uint32_t size;
};

struct tgsi_section {
  uint32_t kind;
  uint32_t resource_id;
  uint32_t virt_sz;
  uint32_t phys_sz;
};

