#ifndef H_MODELMANAGER
#define H_MODELMANAGER

#include "AssetManager.h"
#include "../renderer/Model.h"

class ModelManager : public AssetManager<Model>
{
public:

private:
	virtual Model * PerformCache(const std::string & path);
};

#endif