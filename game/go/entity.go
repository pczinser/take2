components {
  id: "entity"
  component: "/game/scripts/entity.script"
}
embedded_components {
  id: "sprite"
  type: "sprite"
  data: "default_animation: \"idle_south\"\n"
  "material: \"/builtins/materials/sprite.material\"\n"
  "textures {\n"
  "  sampler: \"texture_sampler\"\n"
  "  texture: \"/asset/atlas/player.atlas\"\n"
  "}\n"
  ""
}
