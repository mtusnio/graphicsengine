#ifndef H_LIGHTSOURCE
#define H_LIGHTSOURCE

#include "../math/Vector.h"
#include "../math/Angle.h"


struct LightSource
{
	enum Type
	{
		POINT,
		SPOT
	};

	Vector Position;
	float Color[3];
	float Constant;
	float Linear;
	float Quadratic;
};


struct PointLightSource : public LightSource
{

};

struct SpotLightSource : public LightSource
{
	Angle Rotation;
	float Exponent;
};

#endif