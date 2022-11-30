#version 130
precision mediump float;

varying vec2 vary_uv; //The UV coordinate, going from (0.0, 0.0) to (1.0, 1.0)
uniform sampler2D uTexture; //The texture
void main()
{
vec4 color = texture2D(uTexture, vary_uv); //Apply the texture to this pixel
//Illumination if needed
gl_Color = color;
}
