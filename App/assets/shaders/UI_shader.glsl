#type vertex
#version 450 core

// --- ATTRIBUTES (Must match C++ Layout) ---
// 0: Position, 1: UV, 2: Color, 3: TexIndex, 4: Data1, 5: Data2
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;      // Base Body Color
layout(location = 3) in float a_TexIndex;  // Unused in UI, but part of layout
layout(location = 4) in vec4 a_DataFirst;  // .xy = Dimensions, .z = Hovered, .w = Unused
layout(location = 5) in vec4 a_DataSecond; // .rgba = Border Color

layout(std140, binding = 0) uniform Camera
{
    mat4 u_ViewProjection;
};

// --- OUTPUTS TO FRAGMENT SHADER ---
out vec2 v_TexCoord;
out vec4 v_BaseColor;
out vec2 v_Dimensions;
out float v_Hovered;
out vec4 v_BorderColor;

void main()
{
    // Standard mapping
    v_TexCoord = a_TexCoord * 2.0 - 1.0; 
    v_BaseColor = a_Color;

    // --- UNPACKING "UBER" DATA ---
    v_Dimensions  = a_DataFirst.xy;
    v_Hovered     = a_DataFirst.z;
    
    // We treat DataSecond purely as the Border Color
    v_BorderColor = a_DataSecond;

    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

// --- INPUTS FROM VERTEX SHADER ---
in vec2 v_TexCoord;
in vec4 v_BaseColor;
in vec2 v_Dimensions;
in float v_Hovered;
in vec4 v_BorderColor;

// --- GLOBAL UNIFORMS (Shared by all UI elements) ---
uniform float u_Time;
uniform float u_BorderWidth;  // e.g. 0.05
uniform float u_CornerRadius; // e.g. 0.2

// SDF Function for a rounded box
float roundedBoxSDF(vec2 centerPos, vec2 size, float radius) {
    return length(max(abs(centerPos) - size + radius, 0.0)) - radius;
}

void main() {
    // 1. Setup Coordinates based on Dimensions (from Attribute)
    vec2 uv = v_TexCoord;
    
    // Avoid division by zero if dimensions aren't set
    float aspect = (v_Dimensions.y > 0.0) ? (v_Dimensions.x / v_Dimensions.y) : 1.0;
    
    vec2 pos = uv;
    pos.x *= aspect; 
    vec2 boxSize = vec2(aspect, 1.0); 

    // 2. Calculate Distance Field
    float d = roundedBoxSDF(pos, boxSize, u_CornerRadius);

    // 3. Softening/Anti-aliasing
    float smoothFactor = 0.02;
    float opacity = 1.0 - smoothstep(0.0, smoothFactor, d);
    
    if (opacity < 0.01) discard; 

    // 4. Create the Border
    float borderMask = 1.0 - smoothstep(u_BorderWidth - smoothFactor, u_BorderWidth, abs(d));
    
    // 5. Animations (Sheen + Pulse)
    float sheenPos = tan(u_Time * 1.5) * 4.0; 
    float sheenWidth = 0.5;
    float sheen = smoothstep(sheenPos, sheenPos + sheenWidth, pos.x + pos.y * 0.5);
    sheen *= smoothstep(sheenPos + sheenWidth + 0.2, sheenPos + sheenWidth, pos.x + pos.y * 0.5);
    
    // Pulse is stronger when hovered (using v_Hovered attribute)
    float pulse = (sin(u_Time * 3.0) * 0.5 + 0.5) * 0.3;
    float hoverGlow = v_Hovered * 0.4; 

    // 6. Combine Colors
    // Use v_BaseColor (from attribute) instead of uniform
    vec4 bodyColor = v_BaseColor;
    
    // Logic: Body is transparent (0.2) normally, slightly more opaque if hovered
    bodyColor.a = 0.2 + (v_Hovered * 0.2); 
    bodyColor.rgb += vec3(sheen * 0.5);

    // Border Color (from attribute)
    vec4 finalBorder = v_BorderColor;
    finalBorder.rgb += vec3(pulse + hoverGlow);
    finalBorder.a = 1.0; 

    // Mix Border and Body
    vec4 finalColor = mix(bodyColor, finalBorder, borderMask);

    // Apply main shape AA
    finalColor.a *= opacity;

    o_Color = finalColor;
}