@prism(type='fragment', name='OpenGL Fragment Shader', version='1.0.0', description='Fast3D fragment shader', author='Emill & Prism Team')

@{GLSL_VERSION}

@if(core_opengl || opengles)
out vec4 vOutColor;
@end

@for(i in 0..2)
    @if(o_textures[i])
        @{attr} vec2 vTexCoord@{i};
        @for(j in 0..2)
            @if(o_clamp[i][j])
                @if(j == 0)
                    @{attr} float vTexClampS@{i};
                @else
                    @{attr} float vTexClampT@{i};
                @end
            @end
        @end
    @end
@end

@if(o_fog) @{attr} vec4 vFog;
@if(o_grayscale) @{attr} vec4 vGrayscaleColor;

@for(i in 0..o_inputs)
    @if(o_alpha)
        @{attr} vec4 vInput@{i + 1};
    @else
        @{attr} vec3 vInput@{i + 1};
    @end
@end

@if(o_textures[0]) uniform sampler2D uTex0;
@if(o_textures[1]) uniform sampler2D uTex1;

@if(o_masks[0]) uniform sampler2D uTexMask0;
@if(o_masks[1]) uniform sampler2D uTexMask1;

@if(o_blend[0]) uniform sampler2D uTexBlend0;
@if(o_blend[1]) uniform sampler2D uTexBlend1;

uniform int frame_count;
uniform float noise_scale;

uniform int texture_width[2];
uniform int texture_height[2];
uniform int texture_filtering[2];

@if(o_water)
@{attr} vec2 vWaterUv;
uniform sampler2D uWaterNormal;
uniform sampler2D uWaterScene;
uniform vec2 uWaterScreenSize;
uniform float uWaterTime;
@end

#define TEX_OFFSET(off) @{texture}(tex, texCoord - off / texSize)
#define WRAP(x, low, high) mod((x)-(low), (high)-(low)) + (low)

float random(in vec3 value) {
    float random = dot(sin(value), vec3(12.9898, 78.233, 37.719));
    return fract(sin(random) * 143758.5453);
}

vec4 fromLinear(vec4 linearRGB){
    bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
    vec3 higher = vec3(1.055)*pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);
    vec3 lower = linearRGB.rgb * vec3(12.92);
    return vec4(mix(higher, lower, cutoff), linearRGB.a);
}

vec4 filter3point(in sampler2D tex, in vec2 texCoord, in vec2 texSize) {
    vec2 offset = fract(texCoord*texSize - vec2(0.5));
    offset -= step(1.0, offset.x + offset.y);
    vec4 c0 = TEX_OFFSET(offset);
    vec4 c1 = TEX_OFFSET(vec2(offset.x - sign(offset.x), offset.y));
    vec4 c2 = TEX_OFFSET(vec2(offset.x, offset.y - sign(offset.y)));
    return c0 + abs(offset.x)*(c1-c0) + abs(offset.y)*(c2-c0);
}

@if(o_water)
float waterHeightAt(vec2 uv) {
    return @{texture}(uWaterNormal, uv).x;
}

vec3 waterNormalAt(vec2 uv) {
    vec2 texel = vec2(1.0 / 512.0);
    float base = waterHeightAt(uv);
    float dx = waterHeightAt(uv + vec2(texel.x, 0.0)) - base;
    float dz = waterHeightAt(uv + vec2(0.0, texel.y)) - base;
    return normalize(vec3(dx, 64.0 / (1024.0 * 8.0), dz));
}

vec4 shadeGameWater(vec2 uv) {
    const vec3 underwaterColour = vec3(0.6, 0.9, 0.9);
    vec3 normal = waterNormalAt(uv);
    vec2 screenUv = gl_FragCoord.xy / max(uWaterScreenSize, vec2(1.0));
    vec3 viewVec = normalize(vec3((screenUv - vec2(0.5)) * 0.7, 1.0));
    vec3 lightVec = normalize(vec3(0.35, 0.8, 0.25));
    vec3 reflectedView = reflect(-viewVec, normal);
    float specular = pow(max(0.0, dot(reflectedView, lightVec)), 64.0) * 0.5;
    vec2 dudv = normal.xz;
    vec4 refracted = @{texture}(uWaterScene,
        clamp(screenUv + dudv * 0.065, vec2(0.001), vec2(0.999)));
    vec4 reflected = @{texture}(uWaterScene,
        clamp(vec2(screenUv.x, 1.0 - screenUv.y) + dudv * 0.020, vec2(0.001), vec2(0.999)));
    float fresnel = 0.12 + 0.88 * pow(1.0 - abs(dot(normal, viewVec)), 5.0);
    vec3 colour = mix(refracted.rgb, reflected.rgb, fresnel);
    float slope = clamp(length(dudv) * 850.0, 0.0, 1.0);
    colour = mix(colour, underwaterColour, 0.18);
    colour += slope * vec3(0.06, 0.13, 0.14);
    colour += specular * 1.2;
    colour = mix(colour, colour * underwaterColour, 0.28);
    colour += 0.012 * sin(uWaterTime * 1.7 + uv.x * 19.0 + uv.y * 13.0);
    return vec4(colour, 0.52);
}
@end

vec4 hookTexture2D(in int id, sampler2D tex, in vec2 uv, in vec2 texSize) {
@if(o_three_point_filtering)
    if(texture_filtering[id] == @{FILTER_THREE_POINT}) {
        return filter3point(tex, uv, texSize);
    }
@end
    return @{texture}(tex, uv);
}

#define TEX_SIZE(tex) vec2(texture_width[tex], texture_height[tex])

void main() {
    @for(i in 0..2)
        @if(o_textures[i])
            @{s = o_clamp[i][0]}
            @{t = o_clamp[i][1]}

            vec2 texSize@{i} = TEX_SIZE(@{i});

            @if(!s && !t)
                vec2 vTexCoordAdj@{i} = vTexCoord@{i};
            @else
                @if(s && t)
                    vec2 vTexCoordAdj@{i} = clamp(vTexCoord@{i}, 0.5 / texSize@{i}, vec2(vTexClampS@{i}, vTexClampT@{i}));
                @elseif(s)
                    vec2 vTexCoordAdj@{i} = vec2(clamp(vTexCoord@{i}.s, 0.5 / texSize@{i}.s, vTexClampS@{i}), vTexCoord@{i}.t);
                @else
                    vec2 vTexCoordAdj@{i} = vec2(vTexCoord@{i}.s, clamp(vTexCoord@{i}.t, 0.5 / texSize@{i}.t, vTexClampT@{i}));
                @end
            @end

            vec4 texVal@{i} = hookTexture2D(@{i}, uTex@{i}, vTexCoordAdj@{i}, texSize@{i});

            @if(o_masks[i])
                @if(opengles) 
                    vec2 maskSize@{i} = vec2(textureSize(uTexMask@{i}, 0));
                @else 
                    vec2 maskSize@{i} = textureSize(uTexMask@{i}, 0);
                @end

                vec4 maskVal@{i} = hookTexture2D(@{i}, uTexMask@{i}, vTexCoordAdj@{i}, maskSize@{i});

                @if(o_blend[i])
                    vec4 blendVal@{i} = hookTexture2D(@{i}, uTexBlend@{i}, vTexCoordAdj@{i}, texSize@{i});
                @else
                    vec4 blendVal@{i} = vec4(0, 0, 0, 0);
                @end

                texVal@{i} = mix(texVal@{i}, blendVal@{i}, maskVal@{i}.a);
            @end
        @end
    @end

    @if(o_alpha) 
        vec4 texel;
    @else 
        vec3 texel;
    @end

    @if(o_2cyc)
        @{f_range = 2}
    @else
        @{f_range = 1}
    @end

    @for(c in 0..f_range)
        @if(c == 1)
            @if(o_alpha)
                @if(o_c[c][1][2] == SHADER_COMBINED)
                    texel.a = WRAP(texel.a, -1.01, 1.01);
                @else
                    texel.a = WRAP(texel.a, -0.51, 1.51);
                @end
            @end

            @if(o_c[c][0][2] == SHADER_COMBINED)
                texel.rgb = WRAP(texel.rgb, -1.01, 1.01);
            @else
                texel.rgb = WRAP(texel.rgb, -0.51, 1.51);
            @end
        @end

        @if(!o_color_alpha_same[c] && o_alpha)
            texel = vec4(@{
            append_formula(o_c[c], o_do_single[c][0],
                           o_do_multiply[c][0], o_do_mix[c][0], false, false, true, c == 0)
            }, @{append_formula(o_c[c], o_do_single[c][1],
                           o_do_multiply[c][1], o_do_mix[c][1], true, true, true, c == 0)
            });
        @else
            texel = @{append_formula(o_c[c], o_do_single[c][0],
                           o_do_multiply[c][0], o_do_mix[c][0], o_alpha, false,
                           o_alpha, c == 0)};
        @end
    @end

    texel = WRAP(texel, -0.51, 1.51);
    texel = clamp(texel, 0.0, 1.0);
    // TODO discard if alpha is 0?
    @if(o_fog)
        @if(o_alpha)
            texel = vec4(mix(texel.rgb, vFog.rgb, vFog.a), texel.a);
        @else
            texel = mix(texel, vFog.rgb, vFog.a);
        @end
    @end

    @if(o_texture_edge && o_alpha)
        if (texel.a > 0.19) texel.a = 1.0; else discard;
    @end

    @if(o_alpha && o_noise)
        texel.a *= floor(clamp(random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + texel.a, 0.0, 1.0));
    @end

    @if(o_grayscale)
        float intensity = (texel.r + texel.g + texel.b) / 3.0;
        vec3 new_texel = vGrayscaleColor.rgb * intensity;
        texel.rgb = mix(texel.rgb, new_texel, vGrayscaleColor.a);
    @end

    @if(o_water)
        vec4 waterColour = shadeGameWater(vWaterUv);
        @if(o_alpha)
            texel = waterColour;
        @else
            texel = waterColour.rgb;
        @end
    @end

    @if(o_alpha)
        @if(o_alpha_threshold)
            if (texel.a < 8.0 / 256.0) discard;
        @end
        @if(o_invisible)
            texel.a = 0.0;
        @end
        @{vOutColor} = texel;
    @else
        @{vOutColor} = vec4(texel, 1.0);
    @end

    @if(srgb_mode)
        @{vOutColor} = fromLinear(@{vOutColor});
    @end
}
