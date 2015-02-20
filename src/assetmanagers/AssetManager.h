#ifndef H_ASSETMANAGER
#define H_IASSETMANAGER

#include <string>
#include <unordered_map>
#include <memory>

template<typename T>
class AssetManager
{
public:
	AssetManager();
	virtual ~AssetManager();

	// Caches the asset or returns it if it's already in cache.
	// nullptr if file not found
	std::shared_ptr<const T> Cache(const std::string & path);

	// Acts like the cache function, but doesn't cache, only checks
	// for existings files
	std::shared_ptr<const T> GetAsset(const std::string & path);

private:
	// Derived classes should load the data into an object here and return it
	// for it to be added to the map by the AssetManager
	virtual T * PerformCache(const std::string & path) = 0;


	// Maps our paths to loaded assets.
	// Using shared_ptr/weak_ptr architecture to possibly add dynamic
	// removal of objects once they run out of scope
	std::unordered_map<std::string, std::weak_ptr<T>> m_Assets;
};

template<typename T>
AssetManager<T>::AssetManager()
{
}

template<typename T>
AssetManager<T>::~AssetManager()
{
	
}

template<typename T>
std::shared_ptr<const T> AssetManager<T>::Cache(const std::string & path)
{
	T * asset = GetAsset(path);

	if (asset)
		return asset;

	asset = PerformCache(path);

	if (!asset)
		return nullptr;

	m_Assets[path] = asset;
	return m_Assets[path].lock();
}

template<typename T>
std::shared_ptr<const T> AssetManager<T>::GetAsset(const std::string & path)
{
	auto it = m_Assets.find(path);
	if (it != m_Assets.end())
		return it.ma

	return nullptr;
}




#endif