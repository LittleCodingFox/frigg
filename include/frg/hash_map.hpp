#ifndef FRG_HASHMAP_HPP
#define FRG_HASHMAP_HPP

#include <frg/allocation.hpp>
#include <frg/hash.hpp>
#include <frg/macros.hpp>
#include <frg/tuple.hpp>
#include <frg/optional.hpp>

namespace frg FRG_VISIBILITY {

template<typename Key, typename Value, typename Hash, typename Allocator>
class hash_map {
public:
	typedef tuple<const Key, Value> entry_type;

private:
	struct chain {
		entry_type entry;
		chain *next;

		chain(const Key &new_key, const Value &new_value)
		: entry{new_key, new_value}, next{nullptr} { }

		chain(const Key &new_key, Value &&new_value)
		: entry{new_key, std::move(new_value)}, next{nullptr} { }
	};

public:
	class iterator {
	friend class hash_map;
	public:
		iterator &operator++ () {
			FRG_ASSERT(item);
			item = item->next;
			if(item)
				return *this;

			while(bucket < map._capacity) {
				item = map._table[bucket];
				bucket++;
				if(item)
					break;
			}

			return *this;
		}

		entry_type &operator* () {
			return item->entry;
		}
		entry_type *operator-> () {
			return &item->entry;
		}

		operator bool () {
			return item != nullptr;
		}

	private:
		iterator(hash_map &map, size_t bucket, chain *item)
		: map(map), item(item), bucket(bucket) { }

		hash_map &map;
		chain *item;
		size_t bucket;
	};

	hash_map(const Hash &hasher, Allocator &allocator);
	hash_map(const hash_map &) = delete;

	~hash_map();

	void insert(const Key &key, const Value &value);
	void insert(const Key &key, Value &&value);

	bool empty() {
		return !_size;
	}

	iterator iterator() {
		if(!_size)
			return iterator(*this, _capacity, nullptr);

		for(size_t bucket = 0; bucket < _capacity; bucket++) {
			if(_table[bucket])
				return iterator(*this, bucket, _table[bucket]);
		}
		
		FRG_ASSERT(!"hash_map corrupted");
		__builtin_unreachable();
	}
	
	template<typename KeyCompatible>
	Value *get(const KeyCompatible &key);

	optional<Value> remove(const Key &key);

private:
	void rehash();
	
	Hash _hasher;
	Allocator &_allocator;
	chain **_table;
	size_t _capacity;
	size_t _size;
};

template<typename Key, typename Value, typename Hash, typename Allocator>
hash_map<Key, Value, Hash, Allocator>::hash_map(const Hash &hasher, Allocator &allocator)
: _hasher(hasher), _allocator(allocator), _table(nullptr), _capacity(0), _size(0) { }

template<typename Key, typename Value, typename Hash, typename Allocator>
hash_map<Key, Value, Hash, Allocator>::~hash_map() {
	for(size_t i = 0; i < _capacity; i++) {
		chain *item = _table[i];
		while(item != nullptr) {
			chain *next = item->next;
			frg::destruct(_allocator, item);
			item = next;
		}
	}
	_allocator.free(_table);
}

template<typename Key, typename Value, typename Hash, typename Allocator>
void hash_map<Key, Value, Hash, Allocator>::insert(const Key &key, const Value &value) {
	if(_size >= _capacity)
		rehash();

	FRG_ASSERT(_capacity > 0);
	unsigned int bucket = ((unsigned int)_hasher(key)) % _capacity;
	
	auto item = frg::construct<chain>(_allocator, key, value);
	item->next = _table[bucket];
	_table[bucket] = item;
	_size++;
}
template<typename Key, typename Value, typename Hash, typename Allocator>
void hash_map<Key, Value, Hash, Allocator>::insert(const Key &key, Value &&value) {
	if(_size >= _capacity)
		rehash();

	FRG_ASSERT(_capacity > 0);
	unsigned int bucket = ((unsigned int)_hasher(key)) % _capacity;
	
	auto item = frg::construct<chain>(_allocator, key, std::move(value));
	item->next = _table[bucket];
	_table[bucket] = item;
	_size++;
}

template<typename Key, typename Value, typename Hash, typename Allocator>
template<typename KeyCompatible>
Value *hash_map<Key, Value, Hash, Allocator>::get(const KeyCompatible &key) {
	if(_size == 0)
		return nullptr;

	unsigned int bucket = ((unsigned int)_hasher(key)) % _capacity;

	for(chain *item = _table[bucket]; item != nullptr; item = item->next) {
		if(item->entry.template get<0>() == key)
			return &item->entry.template get<1>();
	}

	return nullptr;
}

template<typename Key, typename Value, typename Hash, typename Allocator>
optional<Value> hash_map<Key, Value, Hash, Allocator>::remove(const Key &key) {
	if(_size == 0)
		return null_opt;

	unsigned int bucket = ((unsigned int)_hasher(key)) % _capacity;
	
	chain *previous = nullptr;
	for(chain *item = _table[bucket]; item != nullptr; item = item->next) {
		if(item->entry.template get<0>() == key) {
			Value value = std::move(item->entry.template get<1>());
			
			if(previous == nullptr) {
				_table[bucket] = item->next;
			}else{
				previous->next = item->next;
			}
			frg::destruct(_allocator, item);
			_size--;

			return value;
		}

		previous = item;
	}

	return null_opt;
}

template<typename Key, typename Value, typename Hash, typename Allocator>
void hash_map<Key, Value, Hash, Allocator>::rehash() {
	size_t new_capacity = 2 * _size;
	if(new_capacity < 10)
		new_capacity = 10;

	chain **new_table = (chain **)_allocator.allocate(sizeof(chain *) * new_capacity);
	for(size_t i = 0; i < new_capacity; i++)
		new_table[i] = nullptr;
	
	for(size_t i = 0; i < _capacity; i++) {
		chain *item = _table[i];
		while(item != nullptr) {
			auto bucket = ((unsigned int)_hasher(item->entry.template get<0>())) % new_capacity;

			chain *next = item->next;
			item->next = new_table[bucket];
			new_table[bucket] = item;
			item = next;
		}
	}

	_allocator.free(_table);
	_table = new_table;
	_capacity = new_capacity;
}

} // namespace frg

#endif // FRG_HASHMAP_HPP