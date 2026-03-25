#type vertex
#version 450 core

// --- ATTRIBUTES ---
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;      // Base Body Color
layout(location = 3) in float a_TexIndex;  
layout(location = 4) in vec4 a_DataFirst;  // .xy = Dimensions, .z = Hovered State
layout(location = 5) in vec4 a_DataSecond; // .rgba = Border Color

layout(std140, binding = 0) uniform Camera
{
    mat4 u_ViewProjection;
};

// --- OUTPUTS ---
out vec2 v_TexCoord;
out vec4 v_BaseColor;
out vec2 v_Dimensions;
out float v_Hovered;
out vec4 v_BorderColor;

void main()
{
    v_TexCoord = a_TexCoord * 2.0 - 1.0; 
    v_BaseColor = a_Color;
    
    v_Dimensions  = a_DataFirst.xy;
    v_Hovered     = a_DataFirst.z;
    v_BorderColor = a_DataSecond;

    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;
in vec4 v_BaseColor;
in vec2 v_Dimensions;
in float v_Hovered;
in vec4 v_BorderColor;

uniform float u_Time;
uniform float u_BorderWidth;  
uniform float u_CornerRadius; 

// --- GLOW / SHADOW CONFIGURATION ---
const float GLOW_OPACITY = 0.8;       // Stronger opacity for visibility
const vec2  GLOW_OFFSET  = vec2(0.0); // (0,0) Centers it to make it a GLOW
const float GLOW_BLUR    = 0.30;      // High blur = wide glow

// IMPORTANT: Padding must be larger than the blur to prevent clipping
// 0.30 means 30% of the quad is empty space for the glow to bleed into
const float PADDING      = 0.30;      

// --- SDF FUNCTION ---
float roundedBoxSDF(vec2 centerPos, vec2 size, float radius) {
    return length(max(abs(centerPos) - size + radius, 0.0)) - radius;
}

void main() {
    // 1. Setup Coordinate Space
    vec2 uv = v_TexCoord; 
    
    float aspect = (v_Dimensions.y > 0.0) ? (v_Dimensions.x / v_Dimensions.y) : 1.0;
    
    vec2 pos = uv;
    pos.x *= aspect; 
    
    // We shrink the actual physical box to make room for the glow
    vec2 boxSize = vec2(aspect, 1.0) - PADDING; 

    // 2. Render Glow (Centered Shadow)
    // Distance field for the glow (centered)
    float dGlow = roundedBoxSDF(pos - GLOW_OFFSET, boxSize, u_CornerRadius);
    
    // Create the glow gradient
    // We fade from -GLOW_BLUR (inside) to +GLOW_BLUR (outside)
    float glowAlpha = 1.0 - smoothstep(-GLOW_BLUR, GLOW_BLUR, dGlow);
    
    // Optional: boost the curve so it's brighter near the object
    glowAlpha = pow(glowAlpha, 1.5); 
    glowAlpha *= GLOW_OPACITY;
    
    // Determine Glow Color
    vec3 glowColorRGB = v_BaseColor.rgb; 
    
    vec4 glowColor = vec4(glowColorRGB, glowAlpha);

    // 3. Render Main Box
    float dBox = roundedBoxSDF(pos, boxSize, u_CornerRadius);
    
    float smoothFactor = 0.02;
    float boxOpacity = 1.0 - smoothstep(0.0, smoothFactor, dBox);

    // 4. Border Logic
    float borderMask = 1.0 - smoothstep(u_BorderWidth - smoothFactor, u_BorderWidth, abs(dBox));
    
    // 5. Animations (Sheen + Pulse)
    float sheenPos = tan(u_Time * 1.5) * 4.0; 
    float sheen = smoothstep(sheenPos, sheenPos + 0.5, pos.x + pos.y * 0.5);
    sheen *= smoothstep(sheenPos + 0.7, sheenPos + 0.5, pos.x + pos.y * 0.5);
    
    float pulse = (sin(u_Time * 3.0) * 0.5 + 0.5) * 0.3;
    float hoverGlow = v_Hovered * 0.4; 

    // 6. Combine Box Colors
    vec4 bodyColor = v_BaseColor;
    bodyColor.a = 0.2 + (v_Hovered * 0.2); 
    bodyColor.rgb += vec3(sheen * 0.5);

    vec4 borderColor = v_BorderColor;
    borderColor.rgb += vec3(pulse + hoverGlow);
    borderColor.a = 1.0; 

    // Mix Body and Border
    vec4 boxFinal = mix(bodyColor, borderColor, borderMask);
    boxFinal.a *= boxOpacity; 

    // 7. Final Composite
    // Layer the Box ON TOP of the Glow
    vec4 finalColor = mix(glowColor, boxFinal, boxFinal.a);

    if (finalColor.a < 0.01) discard; 

    o_Color = finalColor;
}