#ifndef BLEND_ATTRIBUTE_TYPES
#define BLEND_ATTRIBUTE_TYPES

#include <cstdint>

/* See DNA_attribute_types.h amd BKE_attribute */

enum class AttrType : int16_t {
  Bool = 0,
  Int8 = 1,
  Int16_2D = 2,
  Int32 = 3,
  Int32_2D = 4,
  Float = 5,
  Float2 = 6,
  Float3 = 7,
  Float4x4 = 8,
  ColorByte = 9,
  ColorFloat = 10,
  Quaternion = 11,
  String = 12,
  Float4 = 13,
};

enum class AttrDomain : int8_t {
  /* Used to choose automatically based on other data. */
  Auto = -1,
  /* Mesh, Curve or Point Cloud Point. */
  Point = 0,
  /* Mesh Edge. */
  Edge = 1,
  /* Mesh Face. */
  Face = 2,
  /* Mesh Corner. */
  Corner = 3,
  /* A single curve in a larger curve data-block. */
  Curve = 4,
  /* Instance. */
  Instance = 5,
  /* A layer in a grease pencil data-block. */
  Layer = 6,
};

/** Some storage types are only relevant for certain attribute types. */
enum class AttrStorageType : int8_t {
  /** #AttributeDataArray. */
  Array = 0,
  /** A single value for the whole attribute. */
  Single = 1,
};



#endif