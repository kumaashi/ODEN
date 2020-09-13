glslangValidator -V -S frag --D _PS_ ./shaders/clear.glsl -o ./shaders/clear.glsl_PS_temp.spv
glslangValidator -V -S vert --D _VS_ ./shaders/model.glsl -o ./shaders/model.glsl_VS_temp.spv
glslangValidator -V -S frag --D _PS_ ./shaders/model.glsl -o ./shaders/model.glsl_PS_temp.spv
glslangValidator -V -S vert --D _VS_ ./shaders/bloom.glsl -o ./shaders/bloom.glsl_VS_temp.spv
glslangValidator -V -S frag --D _PS_ ./shaders/bloom.glsl -o ./shaders/bloom.glsl_PS_temp.spv
glslangValidator -V -S vert --D _VS_ ./shaders/present.glsl -o ./shaders/present.glsl_VS_temp.spv
glslangValidator -V -S frag --D _PS_ ./shaders/present.glsl -o ./shaders/present.glsl_PS_temp.spv
glslangValidator -V -S vert --D _VS_ ./shaders/showdepth.glsl -o ./shaders/showdepth.glsl_VS_temp.spv
