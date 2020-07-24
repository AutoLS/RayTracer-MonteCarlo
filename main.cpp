#include <windows.h>
#include <time.h>
#include <stdio.h>
#include <AE/math.h>

#include "ray.h"

image_32 CreateImage(int Width, int Height)
{
	image_32 Image = {};
	Image.Width = Width;
	Image.Height = Height;
	Image.BufferSize = sizeof(uint32) * Width * Height;
	Image.Pixels = (uint32*)malloc(Image.BufferSize);
	
	return Image;
}

void FillImage(image_32* Image, uint8 R, uint8 G, uint8 B)
{
	uint32* Pixels = Image->Pixels;
	for(int Y = 0; Y < Image->Height; ++Y)
	{
		for(int X = 0; X < Image->Width; ++X)
		{
			*Pixels++ = 0xFF << 24 | R << 16 | G << 8 | B << 0;
		}
	}
}

void WriteImage(char* FileName, image_32* Image)
{	
	bitmap_header Header = {};
	Header.FileType = 0x4D42;
	Header.FileSize = sizeof(Header) + Image->BufferSize;
	Header.BitmapOffset = sizeof(Header);
	Header.Size = sizeof(Header) - 14;
	Header.Width = Image->Width;
	Header.Height = Image->Height;
	Header.Planes = 1;
	Header.BitsPerPixel = 32;
	Header.Compression = 0;
	Header.SizeOfBitmap = Image->BufferSize;
	Header.HorResolution = 0;
	
	FILE* OutFile = fopen(FileName, "wb");
	if(OutFile)
	{
		fwrite(&Header, sizeof(Header), 1, OutFile);
		fwrite(Image->Pixels, Image->BufferSize, 1, OutFile);
		fclose(OutFile);
		printf("Successfully generated image.");
	}
	else
	{
		printf("Error on writing image... \n");
	}
}

static real32
RandomUnilaterial()
{
	real32 Result = (real32)rand() / (real32)RAND_MAX;
	return Result;
}

static real32
RandomBilaterial()
{
	real32 Result = -1 + 2.0f * RandomUnilaterial();
	return Result;
}

v3 RayCast(world* World, v3 RayOrigin, v3 RayDir)
{
	v3 Result = {};
	v3 Attenuation = V3(1, 1, 1);
	real32 Tolerance = 0.0001f;
	
	for(uint32 BounceCount = 0;
		BounceCount < 8;
		++BounceCount)
	{
		real32 HitDistance = FLT_MAX;
		
		uint32 HitMatIndex = 0;
		v3 NextOrigin = RayOrigin;
		v3 NextNormal = RayDir;
		
		++World->RaysBounced;
		
		for(uint32 PlaneIndex = 0; 
			PlaneIndex < World->PlaneCount; 
			++PlaneIndex)
		{
			plane Plane = World->Planes[PlaneIndex];
			
			real32 Denom = Dot(Plane.N, RayDir);
			if((Denom < -Tolerance) || (Denom > Tolerance))
			{
				real32 t = (-Plane.d - Dot(Plane.N, RayOrigin)) / Denom;
				if((t > Tolerance) && (t < HitDistance))
				{
					HitDistance = t;
					HitMatIndex = Plane.MatIndex;
					
					NextOrigin = RayOrigin + t * RayDir;
					NextNormal = Plane.N;
				}
			}
		}
		
		for(uint32 SphereIndex = 0; 
			SphereIndex < World->SphereCount; 
			++SphereIndex)
		{
			sphere Sphere = World->Spheres[SphereIndex];
			
			v3 SphereRelativeRayOrigin = RayOrigin - Sphere.P;
			
			real32 a = Dot(RayDir, RayDir);
			real32 b = 2 * Dot(SphereRelativeRayOrigin, RayDir);
			real32 c = Dot(SphereRelativeRayOrigin, SphereRelativeRayOrigin) - 
					   (Sphere.r * Sphere.r);
			real32 Denom = 2 * a;
			
			real32 RootTerm = sqrtf((b*b) - (4*a*c));
			if(RootTerm > Tolerance)
			{		
				real32 tp = (-b + RootTerm) / Denom;
				real32 tn = (-b - RootTerm) / Denom;
				
				real32 t = tp;
				if((tn > Tolerance) && (tn < tp))
				{
					t = tn;
				}
				
				if((t > Tolerance) && (t < HitDistance))
				{
					HitDistance = t;
					HitMatIndex = Sphere.MatIndex;
					
					NextOrigin = RayOrigin + t * RayDir;
					NextNormal = Normalize(NextOrigin - Sphere.P);
				}
			}
		}
		
		if(HitMatIndex)
		{
			material Mat = World->Materials[HitMatIndex];
			
			//real32 CosAttenuation = 1.0f;
#if 1
			real32 CosAttenuation = Dot(-RayDir, NextNormal);
			if(CosAttenuation < 0)
				CosAttenuation = 0;
#endif
			Result += Hadamard(Attenuation, Mat.EmitColor);
			Attenuation = Hadamard(Attenuation, CosAttenuation*Mat.RefColor);
			
			RayOrigin = NextOrigin;
			
			v3 RandBounceVec = 
			V3(RandomBilaterial(), RandomBilaterial(), RandomBilaterial());
			
			v3 PureBounce = RayDir - 2 * Project(RayDir, NextNormal);
			v3 RandomBounce = Normalize(NextNormal + RandBounceVec);
			RayDir = Normalize(Lerp(RandomBounce, PureBounce, Mat.Scatter));
		}
		else
		{
			material Mat = World->Materials[HitMatIndex];
			Result += Hadamard(Attenuation, Mat.EmitColor);
			break;
		}
	}
		
	return Result;
}

static real32
ExactLinearTosRGB(real32 L)
{
	if(L < 0)
		L = 0;
	if(L > 1.0f)
		L = 1.0f;
	
	real32 S = L * 12.92f;
	if(L > 0.0031308)
	{
		S = 1.055f * powf(L, 1.0f/2.4f) - 0.055f;
	}
	
	return S;
}

int main(int nArg, char** Args)
{	
	SYSTEMTIME SystemTime = {};
	GetSystemTime(&SystemTime);
	srand(SystemTime.wSecond);

	image_32 Image = CreateImage(1280, 720);
	
	material Materials[10] = {};
	Materials[0].EmitColor = V3(0.3f, 0.5f, 0.5f);
	Materials[1].RefColor = V3(0.5f, 0.5f, 0.5f);
	Materials[2].RefColor = V3(0.7f, 0.5f, 0.3f);
	Materials[3].RefColor = V3(0.3f, 0.8f, 0.3f);
	Materials[3].Scatter = 0.7f;
	Materials[4].EmitColor = V3(4.0f, 4.0f, 4);
	Materials[5].RefColor = V3(0.3f, 0.3f, 0.8f);
	Materials[5].Scatter = 0.85f;
	Materials[6].RefColor = V3(0.3f, 0.7f, 0.9f);
	Materials[7].RefColor = V3(0.5f, 0.8f, 0.3f);
	Materials[8].RefColor = V3(0.2f, 0.5f, 0.3f);
	Materials[9].RefColor = V3(0.5f, 0.3f, 0.8f);
	Materials[6].Scatter = 1.0f;
	Materials[7].Scatter = 0.5f;
	Materials[8].Scatter = 0.6f;
	Materials[9].Scatter = 0.9f;
	
	plane Plane = {};
	Plane.N = V3(0, 0, 1);
	Plane.d = 0;
	Plane.MatIndex = 1;
	
	sphere Spheres[8] = {};
	for(int i = 0; i < ArraySize(Spheres); ++i)
	{
		Spheres[i].P = V3(-13.0f + i*4.0f, 5, Rand32(0.5f, 3));
		Spheres[i].r = Rand32(0.75f, 2.0f);
		Spheres[i].MatIndex = i+2;
	}
#if 0
	Spheres[0].P = V3(0, 0, 0);
	Spheres[0].r = 1.0f;
	Spheres[0].MatIndex = 1;
	
	Spheres[1].P = V3(-2, -1, 1.5f);
	Spheres[1].r = 1.0f;
	Spheres[1].MatIndex = 3;
	
	Spheres[2].P = V3(3, 0, 0);
	Spheres[2].r = 1.0f;
	Spheres[2].MatIndex = 4;
	
	Spheres[3].P = V3(2, 2, 1);
	Spheres[3].r = 1.0f;
	Spheres[3].MatIndex = 5;
#endif
	world World = {};
	World.MaterialCount = ArraySize(Materials);
	World.Materials = Materials;
	World.PlaneCount = 1;
	World.Planes = &Plane;
	World.SphereCount = ArraySize(Spheres);
	World.Spheres = Spheres;
	
	camera Camera = {};
	Camera.P = V3(0, -10, 1);
	Camera.Z = Normalize(V3(0, -1, -0.2f));
	Camera.X = Normalize(Cross(V3(0, 0, 1), Camera.Z));
	Camera.Y = Normalize(Cross(Camera.Z, Camera.X));
	
	real32 FilmDistance = 1.0f;
	v3 FilmCenter = Camera.P - FilmDistance * Camera.Z; 
	real32 FilmWidth = 2.0f;
	real32 FilmHeight = 2.0f;
	
	if(Image.Width > Image.Height)
	{
		FilmHeight = FilmWidth * ((real32)Image.Height / (real32)Image.Width);
	}
	else if(Image.Height > Image.Width)
	{
		FilmWidth = FilmHeight * ((real32)Image.Width / (real32)Image.Height);
	}
	
	real32 HalfFilmW = FilmWidth*0.5f;
	real32 HalfFilmH = FilmHeight*0.5f;
	
	real32 HalfPixW = 0.5f / (real32)Image.Width;
	real32 HalfPixH = 0.5f / (real32)Image.Height;
	
	uint32 RaysPerPixel = 16;
	uint32* Pixels = Image.Pixels;
	
	clock_t LastTime = clock();
	
	for(int Y = 0; Y < Image.Height; ++Y)
	{
		real32 FilmY = -1.0f + 2.0f * ((real32)Y / (real32)Image.Height);
		for(int X = 0; X < Image.Width; ++X)
		{
			real32 FilmX = -1.0f + 2.0f * ((real32)X / (real32)Image.Width);
			
			real32 Contrib = 1.0f / (real32)RaysPerPixel;
			v3 Color = {};
			for(uint32 RayIndex = 0;
				RayIndex < RaysPerPixel;
				++RayIndex)
			{
				real32 OffX = FilmX + RandomBilaterial() * HalfPixW;
				real32 OffY = FilmY + RandomBilaterial() * HalfPixH;
				
				v3 FilmP = FilmCenter + 
						   OffX*HalfFilmW*Camera.X + 
						   OffY*HalfFilmH*Camera.Y;
			
				v3 RayOrigin = Camera.P;
				v3 RayDir = Normalize(FilmP - RayOrigin);
				
				Color += Contrib * RayCast(&World, RayOrigin, RayDir);
			}
			
			v4 BMPValue = 
			{
				255.0f*ExactLinearTosRGB(Color.x),
				255.0f*ExactLinearTosRGB(Color.y),
				255.0f*ExactLinearTosRGB(Color.z),
				255.0f
			};
			*Pixels++ = RGBAPack4x8(BMPValue);
		}
		if( Y % 64 == 0)
		{
			int Percentage = (int)(((real32)Y / (real32)Image.Height) * 100);
			printf("\rRaycasting %d%%...    ", Percentage);
			fflush(stdout);
		}
	}
	
	clock_t EndTime = clock();
	
	real32 SecondsElapsed = ((real32)(EndTime - LastTime)/1000.0f);
	
	printf("Writing image...\n");
	WriteImage("Image.bmp", &Image);
	printf("Seconds elapsed: %.2fs\n", SecondsElapsed);
	printf("Rays computed: %lld\n", World.RaysBounced);
	printf("Performance: %f ms/bounce\n", 
		   (double)(EndTime - LastTime)/(double)World.RaysBounced);
	
	return 0;
}