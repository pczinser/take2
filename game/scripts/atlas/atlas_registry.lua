-- Atlas Registry Module
-- Provides pre-hashed atlas paths for runtime atlas setting

local atlas_registry = {}

-- Pre-hashed atlas paths (compiled .a.texturesetc paths)
local atlas_hashes = {
    ["/asset/atlas/player.atlas"] = hash("/asset/atlas/player.a.texturesetc"),
    ["/asset/atlas/building.atlas"] = hash("/asset/atlas/building.a.texturesetc"),
    -- Add more atlases here as you create them
}

-- Get atlas hash by source path
function atlas_registry.get_atlas_hash(atlas_path)
    local hash_value = atlas_hashes[atlas_path]
    if hash_value then
        return hash_value
    end
    
    print("WARNING: Atlas not found for path:", atlas_path)
    return nil
end

-- Set atlas on a sprite component
function atlas_registry.set_atlas(sprite_url, atlas_path)
    local atlas_hash = atlas_registry.get_atlas_hash(atlas_path)
    if atlas_hash then
        go.set(sprite_url, "image", atlas_hash)
        return true
    end
    return false
end

-- List all available atlases
function atlas_registry.list_atlases()
    print("=== AVAILABLE ATLASES ===")
    for source_path, hash_value in pairs(atlas_hashes) do
        print(string.format("  %s -> %s", source_path, tostring(hash_value)))
    end
    print("=========================")
end

return atlas_registry
