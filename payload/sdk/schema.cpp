#include "schema.h"
#include <Windows.h>
#include <unordered_map>
#include <string>
#include <cstring>


#pragma pack(push, 1)
struct SchemaField
{
    const char* name;          // 0x00
    void*       type;          // 0x08
    uint32_t    offset;        // 0x10
    uint32_t    metadata_size; // 0x14
    void*       metadata;      // 0x18
};

struct SchemaClass
{
    char        pad0[0x08];
    const char* name;          // 0x08
    const char* module_name;   // 0x10
    uint32_t    size;          // 0x18
    uint16_t    num_fields;    // 0x1C
    char        pad1[0x02];    // 0x1E
    uint16_t    static_size;   // 0x20
    uint16_t    metadata_size; // 0x22
    char        pad2[0x04];    // 0x24
    SchemaField* fields;       // 0x28
};

struct SchemaDeclaredClass
{
    char        pad0[0x08];
    const char* name;          // 0x08
    const char* module_name;   // 0x10
    const char* unknown_str;   // 0x18
    SchemaClass* schema_class; // 0x20
};

struct SchemaDeclaredClassEntry
{
    uint64_t             hash[2];         // 0x00
    SchemaDeclaredClass* declared_class;  // 0x10
};

struct SchemaTypeScope
{
    char     pad0[0x08];       // vtable
    char     name[256];        // 0x08
    char     pad1[0x368];      // 0x108
    uint16_t num_classes;      // 0x470
    char     pad2[0x06];       // 0x472
    SchemaDeclaredClassEntry* classes; // 0x478
};

struct SchemaSystem
{
    char             pad0[0x190];
    int              scope_count;    // 0x190
    char             pad1[0x04];     // 0x194
    SchemaTypeScope** scopes;        // 0x198
};
#pragma pack(pop)

// cache: "class::field" -> offset
static std::unordered_map<uint64_t, uint32_t>* g_cache = nullptr;

// simple hash for cache keys
static uint64_t hash_key( const char* a, const char* b )
{
    uint64_t h = 0xcbf29ce484222325ULL;
    while ( *a ) { h ^= (uint8_t)*a++; h *= 0x100000001b3ULL; }
    h ^= ':'; h *= 0x100000001b3ULL;
    h ^= ':'; h *= 0x100000001b3ULL;
    while ( *b ) { h ^= (uint8_t)*b++; h *= 0x100000001b3ULL; }
    return h;
}

bool schema::init()
{
    auto mod = GetModuleHandleA( "schemasystem.dll" );
    if ( !mod ) return false;

    using create_fn = void*( * )( const char*, int* );
    auto create_interface = reinterpret_cast<create_fn>( GetProcAddress( mod, "CreateInterface" ) );
    if ( !create_interface ) return false;

    auto system = reinterpret_cast<SchemaSystem*>( create_interface( "SchemaSystem_001", nullptr ) );
    if ( !system ) return false;

    g_cache = new std::unordered_map<uint64_t, uint32_t>();

    // iterate all scopes (modules)
    for ( int i = 0; i < system->scope_count; i++ )
    {
        auto scope = system->scopes[i];
        if ( !scope || !scope->classes )
            continue;

        if ( strcmp( scope->name, "client.dll" ) != 0 )
            continue;

        for ( uint16_t j = 0; j < scope->num_classes; j++ )
        {
            auto& entry = scope->classes[j];
            if ( !entry.declared_class )
                continue;

            auto declared = entry.declared_class;
            if ( !declared->name || !declared->schema_class )
                continue;

            auto cls = declared->schema_class;
            if ( !cls->fields || cls->num_fields == 0 )
                continue;

            const char* class_name = declared->name;

            for ( uint16_t k = 0; k < cls->num_fields; k++ )
            {
                auto& field = cls->fields[k];
                if ( !field.name )
                    continue;

                uint64_t key = hash_key( class_name, field.name );
                ( *g_cache )[key] = field.offset;
            }
        }
    }

    return g_cache->size() > 0;
}

uint32_t schema::get( const char* module, const char* class_name, const char* field_name )
{
    (void)module;

    if ( !g_cache ) return 0;

    uint64_t key = hash_key( class_name, field_name );
    auto it = g_cache->find( key );
    return ( it != g_cache->end() ) ? it->second : 0;
}
