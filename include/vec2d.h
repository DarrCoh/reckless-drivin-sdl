#ifndef __VEC2D
#define __VEC2D

#include <math.h>
#include "compat.h"

typedef struct{
	float x,y;
} t2DPoint;

static inline t2DPoint VEC2D_Sum(t2DPoint v1,t2DPoint v2)
{
	t2DPoint result;
	result.x=v1.x+v2.x;
	result.y=v1.y+v2.y;
	return result;
}

static inline t2DPoint VEC2D_Difference(t2DPoint v1,t2DPoint v2)
{
	t2DPoint result;
	result.x=v1.x-v2.x;
	result.y=v1.y-v2.y;
	return result;
}

static inline t2DPoint VEC2D_Scale(t2DPoint v,float k)
{
	t2DPoint result;
	result.x=v.x*k;
	result.y=v.y*k;
	return result;
}

static inline t2DPoint P2D(float x,float y)
{
	t2DPoint result;
	result.x=x;
	result.y=y;
	return result;
}

static inline t2DPoint VEC2D_CP(t2DPoint v)
{
	return P2D(v.y,-v.x);
}

static inline float VEC2D_DotProduct(t2DPoint v1,t2DPoint v2)
{
    return (v1.x*v2.x+v1.y*v2.y);	
}

static inline float VEC2D_CrossProduct(t2DPoint v1,t2DPoint v2)
{
    return (v1.x*v2.y-v1.y*v2.x);	
}

static inline float VEC2D_Value(t2DPoint v)
{
	return sqrt(v.x*v.x+v.y*v.y);
}

static inline t2DPoint VEC2D_Norm(t2DPoint v)
{
	float val=VEC2D_Value(v);
	if(val)
		return VEC2D_Scale(v,1/val);
	else 
		return P2D(0,0);
}

#endif