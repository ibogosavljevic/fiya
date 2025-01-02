
#pragma once

#include <unordered_set>
#include <cstddef>
#include <cstring>
#include <cassert>

namespace fiya {

/** @brief String database, used to efficiently store strings
  *        with minimum memory fragmentation.
  * 
  * Same strings are stored only once.
  */
class string_db_t {
public:
    /** @brief Constructor */
    string_db_t(size_t capacity = 2048) {
        m_data = reinterpret_cast<char*>(malloc(capacity * sizeof(char)));
        if (!m_data) {
            throw std::bad_alloc();
        }

        m_capacity = capacity;
        m_size = 0;
    }

    /** Copy constructor */
    string_db_t(const string_db_t& other) :
        m_dict(other.m_dict),
        m_data(nullptr),
        m_size(other.m_size),
        m_capacity(other.m_capacity)
    {
        m_data = reinterpret_cast<char*>(malloc(m_capacity * sizeof(char)));
        if (!m_data) {
            throw std::bad_alloc();
        }

        memcpy(m_data, other.m_data, m_size * sizeof(char));
    }

    /** @brief Destructor */
    ~string_db_t() {
        if (m_data) {
            free(m_data);
        }
    }

    /**
     * @brief Pushes a string into the database, if it doesn't exist yet.
     *        Then, returns the index pointing to the string.
     *
     * @note Pushing a string to the database can potentially invalidate
     *       all the string pointers obtained through method @get. Therefore,
     *       store the string index instead of string pointer.
     * 
     * @param str String to push.
     * @return size_t Returns the index where the string starts. 
     */
    size_t push_back(const char * str) {
        auto it = m_dict.find(hashset_entry_t(str, m_data));
        if (it != m_dict.end()) {
            return it->get_idx();
        } else {
            size_t string_len = strlen(str) + 1;
            if ((string_len + m_size) > m_capacity) {
                /* We don't have enough place in our database to store the string, 
                 * we need to regrow the m_data to store it.
                 */
                size_t new_capacity = std::max<size_t>(m_capacity * 1.5, string_len + m_size);
                char* new_data = reinterpret_cast<char*>(realloc(m_data, new_capacity * sizeof(char)));
                if (new_data == nullptr) {
                    throw std::bad_alloc();
                }

                if (new_data != m_data) {
                    std::memcpy(new_data, m_data, m_size * sizeof(char));    
                    free(m_data);
                }

                m_data = new_data;
                m_capacity = new_capacity;
            }

            std::memcpy(m_data + m_size, str, string_len * sizeof(char));
            size_t string_begin = m_size;
            m_size += string_len;

            m_dict.emplace(string_begin, str);
            return string_begin;
        }
    }

    /** 
     * @brief Returns the string given its index.
     * @note Calling @push_back can invalidate this pointer.
     */
    const char * get(size_t idx) const {
        return m_data + idx;
    } 
private:
    struct hash;

    /** @brief Hashset entry used to lookup strings.
     *
     *    There is a trick involved here. The hash map
     *    only stores index of the string, not the base
     *    pointer (since it can change).
     *
     *    When a lookup is performed, it is done using
     *    the operator==. The other instance of hashset_entry_t
     *    carries the base address as well as pointer to the
     *    valid string. Only then can the two strings be compared.
     *    This implies there are two types of hashset_entry_t, one
     *    that stores only the index, which is stored in the hashset,
     *    and the other that stores the base address of the string
     *    database and the valid string itself, which is used from
     *    the outside for the lookup. 
     *
     *    Therefore, hashset_entry_t is a tagged union.
     * 
     */
    class hashset_entry_t {
    public:
        /* Constructor for the value we use for the lookup. */
        hashset_entry_t(const char * string, const char * base_address) :
            m_type(type_t::STRING_SET),
            m_string(string),
            m_base_address(base_address) {}
        
        /* Constructor for the value stored in the hash map */
        hashset_entry_t(size_t idx, const char * str) :
            m_type(type_t::IDX_SET),
            m_idx(idx),
            m_hash(calculate_hash(str)) {}

        /* Returns the string from the hashset entry. */
        const char * get_string() const {
            assert(m_type == type_t::STRING_SET);
            return m_string;
        }
        /** Returns index of the string */
        size_t get_idx() const {
            assert(m_type == type_t::IDX_SET);
            return m_idx;
        }

        /** @brief operator==. This operator compares an entry
         *      comming from the outsite and has m_type STRING_SET with
         *      an entry in the hashset which has m_type IDX_SET. */
        bool operator==(const hashset_entry_t& other) const {
            const char * str0, * str1;

            if (m_type == type_t::IDX_SET) {
                if (other.m_type == type_t::STRING_SET) {
                    str0 = other.m_string;
                    str1 = other.m_base_address + m_idx;
                } else {
                    assert(false && "Either this or other must be of type_t::STRING_SET");
                }
            } else {
                str0 = m_string;
                if (other.m_type == type_t::IDX_SET) {
                    str1 = m_base_address + other.m_idx;
                } else {
                    str1 = other.m_string;
                }
            }
            
            return strcmp(str0, str1) == 0;
        }

    private:
        /** Type of the hashset_entry_t  */
        enum type_t {
            /** STRING_SET is provided from the outside,
             *  when doing the lookup.
             */
            STRING_SET,
            /** IDX_SET is stored in the std::unordered_set. */
            IDX_SET
        };

        /** Type of this hashset_entry_t, among two possibilites
         *  listed above.
         */
        type_t m_type;

        union {
            /** Use this when m_type is STRING_SET */
            struct {
                const char * m_string;
                const char * m_base_address;
            };
            /** Use this when m_type is IDX_SET */
            struct {
                size_t m_idx;
                size_t m_hash;
            };
        };

        /** @brief Calculates hash value for a given string. */
        static size_t calculate_hash(const char * str) {
            std::size_t hash = 5381;
            char c;

            while ((c = *str++)) {
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
            }

            return hash;
        }

        friend struct hash;
    };

    /** We need to implement hashset calculation for std::unordered_set */
    struct hash {
        std::size_t operator()(const fiya::string_db_t::hashset_entry_t& k) const {
            if (k.m_type == hashset_entry_t::type_t::STRING_SET) {
                return hashset_entry_t::calculate_hash(k.get_string());
            } else {
                return k.m_hash;
            }
        }
    };

    /** Dictionary used for looking up strings in the string_db_t.
     *  We don't want to store duplicate strings.
     */
    std::unordered_set<hashset_entry_t, hash> m_dict;
    /** Pointer to the beginning of the string database. The strings
     *  are stored continously, after one string ends, another begins,
     *  e.g. "foo\0bar\0", where index of foo is 0 and index of bar is 4.
     */
    char * m_data;
    /** Current size of all strings in m_data */
    size_t m_size;
    /** Capacity (maximal size) that can be stored in m_data */
    size_t m_capacity;
};


}
