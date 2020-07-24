#ifndef RAY_H
#define RAY_H

#define ArraySize(arr) (sizeof(arr)/sizeof(arr[0]))

#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 Reserved1;
	uint16 Reserved2;
	uint32 BitmapOffset;
	uint32 Size;
	int Width;
	int Height;
	uint16 Planes;
	uint16 BitsPerPixel;
	uint32 Compression;
	uint32 SizeOfBitmap;
	int HorResolution;
	int VertResolution;
	uint32 ColorsUsed;
	uint32 ColorsImportant;	
};
#pragma pack(pop)

struct image_32
{
	int Width;
	int Height;
	int BufferSize;
	uint32* Pixels;
};

struct material
{
	real32 Scatter; //NOTE: 1 = reflect, 0 = diffuse
	v3 EmitColor;
	v3 RefColor;
};

struct plane
{
	v3 N;
	real32 d;
	uint32 MatIndex;
};

struct sphere
{
	v3 P;
	real32 r;
	uint32 MatIndex;
};

struct camera
{
	v3 P;
	v3 Z;
	v3 Y;
	v3 X;
};

struct world
{
	uint32 MaterialCount;
	material* Materials;
	
	uint32 PlaneCount;
	plane* Planes;
	
	uint32 SphereCount;
	sphere* Spheres;
	
	uint64 RaysBounced;
};

#endif