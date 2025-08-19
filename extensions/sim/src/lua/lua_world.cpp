#include <dmsdk/sdk.h>
#include "../world/world.hpp"
#include "lua_bindings.hpp"
#include "../world/entity.hpp" // Added for chunk-based entity management
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath>

// Add inventory system bindings
#include "../systems/inventory_system.hpp"

namespace simcore {
static int L_spawn_floor_at_z(lua_State* L){
    int32_t z=(int32_t)luaL_checkinteger(L,1);
    int32_t cw=(int32_t)luaL_checkinteger(L,2);
    int32_t ch=(int32_t)luaL_checkinteger(L,3);
    int32_t tw=(int32_t)luaL_checkinteger(L,4);
    int32_t th=(int32_t)luaL_checkinteger(L,5);
    lua_pushinteger(L, SpawnFloorAtZ(z,cw,ch,tw,th)); return 1;
}
static int L_get_active_chunks(lua_State* L){
    int32_t z=(int32_t)luaL_checkinteger(L,1);
    Floor* f=GetFloorByZ(z); lua_newtable(L); if(!f) return 1;
    lua_newtable(L); int i=1;
    for(auto key:f->hot_chunks){ int16_t cx=(int16_t)(key>>32), cy=(int16_t)(key&0xffffffff);
        lua_createtable(L,2,0); lua_pushinteger(L,cx); lua_rawseti(L,-2,1); lua_pushinteger(L,cy); lua_rawseti(L,-2,2); lua_rawseti(L,-2,i++); }
    lua_setfield(L,-2,"hot");
    lua_newtable(L); i=1;
    for(auto key:f->warm_chunks){ int16_t cx=(int16_t)(key>>32), cy=(int16_t)(key&0xffffffff);
        lua_createtable(L,2,0); lua_pushinteger(L,cx); lua_rawseti(L,-2,1); lua_pushinteger(L,cy); lua_rawseti(L,-2,2); lua_rawseti(L,-2,i++); }
    lua_setfield(L,-2,"warm"); return 1;
}

// Fix the create_entity function
static int L_create_entity(lua_State* L) {
    const char* template_name = luaL_checkstring(L, 1);
    float grid_x = (float)luaL_checknumber(L, 2);
    float grid_y = (float)luaL_checknumber(L, 3);
    int32_t floor_z = (int32_t)luaL_optinteger(L, 4, 0);
    
    int32_t entity_id = CreateEntity(template_name, grid_x, grid_y, floor_z);
    lua_pushinteger(L, entity_id);
    return 1;
}

static int L_get_entity(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    Entity* entity = GetEntity(id);
    
    if(!entity) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushinteger(L, entity->id);
    lua_setfield(L, -2, "id");
    
    // Add string fields
    lua_pushstring(L, entity->category.c_str());
    lua_setfield(L, -2, "category");
    lua_pushstring(L, entity->type.c_str());
    lua_setfield(L, -2, "type");
    lua_pushstring(L, entity->name.c_str());
    lua_setfield(L, -2, "name");
    
    // Add grid coordinates (MISSING!)
    lua_pushnumber(L, entity->grid_x);
    lua_setfield(L, -2, "grid_x");
    lua_pushnumber(L, entity->grid_y);
    lua_setfield(L, -2, "grid_y");
    
    // Add chunk coordinates
    lua_pushinteger(L, entity->chunk_x);
    lua_setfield(L, -2, "chunk_x");
    lua_pushinteger(L, entity->chunk_y);
    lua_setfield(L, -2, "chunk_y");
    
    // Add floor_z
    lua_pushinteger(L, entity->floor_z);
    lua_setfield(L, -2, "floor_z");
    
    // Add is_dirty flag
    lua_pushboolean(L, entity->is_dirty);
    lua_setfield(L, -2, "is_dirty");
    
    // Add properties
    lua_newtable(L);
    for(const auto& [key, value] : entity->properties) {
        lua_pushstring(L, key.c_str());
        lua_pushnumber(L, value);
        lua_settable(L, -3);
    }
    lua_setfield(L, -2, "properties");
    
    // Add int_properties
    lua_newtable(L);
    for(const auto& [key, value] : entity->int_properties) {
        lua_pushstring(L, key.c_str());
        lua_pushinteger(L, value);
        lua_settable(L, -3);
    }
    lua_setfield(L, -2, "int_properties");
    
    // Add width and height
    lua_pushinteger(L, entity->width);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, entity->height);
    lua_setfield(L, -2, "height");
    
    // Add inventory_ids
    lua_newtable(L);
    for(size_t i = 0; i < entity->inventory_ids.size(); ++i) {
        lua_pushinteger(L, entity->inventory_ids[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "inventory_ids");
    
    return 1;
}

static int L_move_entity(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    float dx = (float)luaL_checknumber(L, 2);  // CHANGED: grid movement
    float dy = (float)luaL_checknumber(L, 3);  // CHANGED: grid movement
    
    MoveEntity(id, dx, dy);  // CHANGED: grid movement
    return 0;
}

static int L_register_entity_templates(lua_State* L) {
    // Expect a table on the stack
    if(!lua_istable(L, 1)) {
        luaL_error(L, "Expected table for entity templates");
        return 0;
    }
    
    // Iterate through the table
    lua_pushnil(L); // First key
    while(lua_next(L, 1) != 0) {
        // Key is at index -2, value at index -1
        const char* template_name = lua_tostring(L, -2);
        
        if(!lua_istable(L, -1)) {
            printf("Template %s is not a table\n", template_name);
            lua_pop(L, 1);
            continue;
        }
        
        // Create const template from Lua data
        EntityTemplate templ = CreateTemplateFromLua(L, -1);
        RegisterEntityTemplate(template_name, templ);
        
        lua_pop(L, 1);
    }
    
    return 0;
}

// Helper function to create const template from Lua table
EntityTemplate CreateTemplateFromLua(lua_State* L, int table_index) {
    std::string type = "building";  // default
    int32_t width = 1, height = 1;
    std::unordered_map<std::string, float> properties;
    std::unordered_map<std::string, int32_t> int_properties;
    
    // Read type
    lua_getfield(L, table_index, "type");
    if(lua_isstring(L, -1)) {
        type = lua_tostring(L, -1);
    }
    lua_pop(L, 1);
    
    // Read properties
    lua_getfield(L, table_index, "properties");
    if(lua_istable(L, -1)) {
        ReadPropertiesFromLua(L, -1, properties);
    }
    lua_pop(L, 1);
    
    // Read int_properties
    lua_getfield(L, table_index, "int_properties");
    if(lua_istable(L, -1)) {
        ReadIntPropertiesFromLua(L, -1, int_properties);
    }
    lua_pop(L, 1);
    
    return EntityTemplate(type, width, height, properties, int_properties);
}

static int L_get_entities_in_chunk(lua_State* L) {
    int32_t z = (int32_t)luaL_checkinteger(L, 1);
    int32_t chunk_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t chunk_y = (int32_t)luaL_checkinteger(L, 3);
    
    auto entities = GetEntitiesInChunk(z, chunk_x, chunk_y);
    
    lua_newtable(L);
    for(int i = 0; i < (int)entities.size(); i++) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

static int L_get_entities_in_radius(lua_State* L) {
    float grid_x = (float)luaL_checknumber(L, 1);  // CHANGED: grid coordinates
    float grid_y = (float)luaL_checknumber(L, 2);  // CHANGED: grid coordinates
    float radius = (float)luaL_checknumber(L, 3);
    
    auto entities = GetEntitiesInRadius(grid_x, grid_y, radius);  // CHANGED: grid coordinates
    
    lua_newtable(L);
    for(int i = 0; i < (int)entities.size(); i++) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

static int L_set_entity_position(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    float grid_x = (float)luaL_checknumber(L, 2);  // CHANGED: grid coordinates
    float grid_y = (float)luaL_checknumber(L, 3);  // CHANGED: grid coordinates
    
    SetEntityPosition(id, grid_x, grid_y);  // CHANGED: grid coordinates
    return 0;
}

static int L_set_current_floor(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    SetCurrentFloor(floor_z);
    return 0;
}

static int L_get_current_floor(lua_State* L) {
    int32_t floor_z = GetCurrentFloor();
    lua_pushinteger(L, floor_z);
    return 1;
}

static int L_get_entities_on_floor(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    std::vector<int32_t> entities = GetEntitiesOnFloor(floor_z);
    
    lua_newtable(L);
    for(size_t i = 0; i < entities.size(); ++i) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int L_set_entity_floor(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 2);
    SetEntityFloor(id, floor_z);
    return 0;
}

static int L_can_create_chunk_on_floor(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    int32_t chunk_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t chunk_y = (int32_t)luaL_checkinteger(L, 3);
    bool can_create = CanCreateChunkOnFloor(floor_z, chunk_x, chunk_y);
    lua_pushboolean(L, can_create);
    return 1;
}

static int L_get_floor_max_chunks(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    int32_t max_chunks = GetFloorMaxChunks(floor_z);
    lua_pushinteger(L, max_chunks);
    return 1;
}

static int L_get_entity_occupied_tiles(lua_State* L) {
    int32_t entity_id = (int32_t)luaL_checkinteger(L, 1);
    std::vector<std::pair<int32_t, int32_t>> tiles = GetEntityOccupiedTiles(entity_id);
    
    lua_newtable(L);
    for(size_t i = 0; i < tiles.size(); ++i) {
        lua_newtable(L);
        lua_pushinteger(L, tiles[i].first);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, tiles[i].second);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int L_get_entity_occupied_chunks(lua_State* L) {
    int32_t entity_id = (int32_t)luaL_checkinteger(L, 1);
    std::vector<std::pair<int32_t, int32_t>> chunks = GetEntityOccupiedChunks(entity_id);
    
    lua_newtable(L);
    for(size_t i = 0; i < chunks.size(); ++i) {
        lua_newtable(L);
        lua_pushinteger(L, chunks[i].first);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, chunks[i].second);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int L_get_entities_at_tile(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    int32_t tile_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t tile_y = (int32_t)luaL_checkinteger(L, 3);
    std::vector<int32_t> entities = GetEntitiesAtTile(floor_z, tile_x, tile_y);
    
    lua_newtable(L);
    for(size_t i = 0; i < entities.size(); ++i) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// Add tile resource functions
static int L_get_tile_resources(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    int32_t tile_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t tile_y = (int32_t)luaL_checkinteger(L, 3);
    
    Tile* tile = GetTile(floor_z, tile_x, tile_y);
    if(!tile) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushnumber(L, tile->stone_amount);
    lua_setfield(L, -2, "stone");
    lua_pushnumber(L, tile->iron_amount);
    lua_setfield(L, -2, "iron");
    lua_pushnumber(L, tile->wood_amount);
    lua_setfield(L, -2, "wood");
    lua_pushboolean(L, tile->excavated);
    lua_setfield(L, -2, "excavated");
    
    return 1;
}

static int L_initialize_tile_resources(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    int32_t tile_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t tile_y = (int32_t)luaL_checkinteger(L, 3);
    float stone_amount = (float)luaL_checknumber(L, 4);
    
    InitializeTileResources(floor_z, tile_x, tile_y, stone_amount);
    return 0;
}

// Add function to register default templates
static int L_register_default_templates(lua_State* L) {
    RegisterDefaultEntityTemplates();
    return 0;
}

static int L_get_all_entities(lua_State* L) {
    const std::vector<Entity>& entities = GetAllEntities();
    
    lua_newtable(L);
    for(size_t i = 0; i < entities.size(); ++i) {
        lua_newtable(L);
        
        lua_pushinteger(L, entities[i].id);
        lua_setfield(L, -2, "id");
        lua_pushstring(L, entities[i].category.c_str());
        lua_setfield(L, -2, "category");
        lua_pushstring(L, entities[i].type.c_str());
        lua_setfield(L, -2, "type");
        lua_pushstring(L, entities[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushnumber(L, entities[i].grid_x);
        lua_setfield(L, -2, "grid_x");
        lua_pushnumber(L, entities[i].grid_y);
        lua_setfield(L, -2, "grid_y");
        
        // Add properties
        lua_newtable(L);
        for(const auto& [key, value] : entities[i].properties) {
            lua_pushstring(L, key.c_str());
            lua_pushnumber(L, value);
            lua_settable(L, -3);
        }
        lua_setfield(L, -2, "properties");
        
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// Add inventory system bindings
static int L_inventory_create(lua_State* L) {
    int32_t type = (int32_t)luaL_checkinteger(L, 1);
    int32_t capacity = (int32_t)luaL_checkinteger(L, 2);
    int32_t id = Inventory_Create((InventoryType)type, capacity);
    lua_pushinteger(L, id);
    return 1;
}

static int L_inventory_add_items(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t item = (int32_t)luaL_checkinteger(L, 2);
    int32_t amount = (int32_t)luaL_checkinteger(L, 3);
    bool success = Inventory_AddItems(id, (ItemType)item, amount);
    lua_pushboolean(L, success);
    return 1;
}

static int L_inventory_get_item_count(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t item = (int32_t)luaL_checkinteger(L, 2);
    int32_t count = Inventory_GetItemCount(id, (ItemType)item);
    lua_pushinteger(L, count);
    return 1;
}

static int L_inventory_get_stats(lua_State* L) {
    InventoryStats stats = Inventory_GetStats();
    lua_pushinteger(L, stats.total_inventories);
    lua_pushinteger(L, stats.total_items);
    lua_pushinteger(L, stats.total_capacity);
    return 3;
}

static int L_inventory_get_capacity(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t capacity = Inventory_GetCapacity(id);
    lua_pushinteger(L, capacity);
    return 1;
}

static int L_inventory_get_free_space(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t free_space = Inventory_GetFreeSpace(id);
    lua_pushinteger(L, free_space);
    return 1;
}

static int L_inventory_remove_items(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t item = (int32_t)luaL_checkinteger(L, 2);
    int32_t amount = (int32_t)luaL_checkinteger(L, 3);
    bool success = Inventory_RemoveItems(id, (ItemType)item, amount);
    lua_pushboolean(L, success);
    return 1;
}

static int L_inventory_transfer_items(lua_State* L) {
    int32_t from_id = (int32_t)luaL_checkinteger(L, 1);
    int32_t to_id = (int32_t)luaL_checkinteger(L, 2);
    int32_t item = (int32_t)luaL_checkinteger(L, 3);
    int32_t amount = (int32_t)luaL_checkinteger(L, 4);
    bool success = Inventory_TransferItems(from_id, to_id, (ItemType)item, amount);
    lua_pushboolean(L, success);
    return 1;
}

static int L_inventory_has_items(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t item = (int32_t)luaL_checkinteger(L, 2);
    int32_t amount = (int32_t)luaL_checkinteger(L, 3);
    bool has_items = Inventory_HasItems(id, (ItemType)item, amount);
    lua_pushboolean(L, has_items);
    return 1;
}

static int L_assign_inventory_to_entity(lua_State* L) {
    int32_t entity_id = (int32_t)luaL_checkinteger(L, 1);
    int32_t inventory_id = (int32_t)luaL_checkinteger(L, 2);
    
    Entity* entity = GetEntity(entity_id);
    if(entity) {
        entity->inventory_ids.push_back(inventory_id);
        printf("Assigned inventory %d to entity %d\n", inventory_id, entity_id);
        lua_pushboolean(L, true);
    } else {
        printf("ERROR: Entity %d not found\n", entity_id);
        lua_pushboolean(L, false);
    }
    return 1;
}

void LuaRegister_World(lua_State* L){
    static const luaL_reg API[]={
        {"spawn_floor_at_z",L_spawn_floor_at_z},
        {"get_active_chunks",L_get_active_chunks},
        {"create_entity", L_create_entity},
        {"get_entity", L_get_entity},
        {"move_entity", L_move_entity},
        {"register_entity_templates", L_register_entity_templates},
        {"get_entities_in_chunk", L_get_entities_in_chunk},
        {"get_entities_in_radius", L_get_entities_in_radius},
        {"set_entity_position", L_set_entity_position},
        {"set_entity_floor", L_set_entity_floor},
        {"set_current_floor", L_set_current_floor},
        {"get_current_floor", L_get_current_floor},
        {"get_entities_on_floor", L_get_entities_on_floor},
        {"can_create_chunk_on_floor", L_can_create_chunk_on_floor},
        {"get_floor_max_chunks", L_get_floor_max_chunks},
        {"get_entity_occupied_tiles", L_get_entity_occupied_tiles},
        {"get_entity_occupied_chunks", L_get_entity_occupied_chunks},
        {"get_entities_at_tile", L_get_entities_at_tile},
        {"get_tile_resources", L_get_tile_resources},
        {"initialize_tile_resources", L_initialize_tile_resources},
        {"get_all_entities", L_get_all_entities},
        {"register_default_templates", L_register_default_templates},
        {"inventory_create", L_inventory_create},
        {"inventory_add_items", L_inventory_add_items},
        {"inventory_get_item_count", L_inventory_get_item_count},
        {"inventory_get_stats", L_inventory_get_stats},
        {"inventory_get_capacity", L_inventory_get_capacity},
        {"inventory_get_free_space", L_inventory_get_free_space},
        {"inventory_remove_items", L_inventory_remove_items},
        {"inventory_transfer_items", L_inventory_transfer_items},
        {"inventory_has_items", L_inventory_has_items},
        {"assign_inventory_to_entity", L_assign_inventory_to_entity},
        {0,0}
    };
    int top=lua_gettop(L); lua_getglobal(L,"sim"); if(lua_isnil(L,-1)){ lua_pop(L,1); lua_newtable(L); }
    luaL_register(L,0,API); lua_setglobal(L,"sim"); (void)top;
}
} // namespace simcore
