#version 130
precision mediump float;

attribute vec3 vPosition; //Depending who compiles, these variables are not "attribute" but "in". In this version (130) both are accepted. in should be used later
attribute vec3 vColor;
attribute vec2 vUV;
attribute vec3 vNormal;


//uniform float uScale;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uInvModel3x3;

varying vec4 varyColor; //Depending who compiles, these variables are not "varying" but "out". In this version (130) both are accepted. out should be used later
varying vec2 vary_uv;
varying vec3 vary_normal;
varying vec4 vary_world_position;

//We still use varying because OpenGLES 2.0 (OpenGL Embedded System, for example for smartphones) does not accept "in" and "out"

void main()
{
      //gl_Position = vec4(uScale*vPosition, 1.0); We need to put vPosition as a vec4. Because vPosition is a vec3, we need one more value (w) which is here 1.0. Hence x and y go from -w to w hence -1 to +1. Premultiply this variable if you want to transform the position.
      gl_Position = uMVP*vec4(vPosition, 1.0);
      //gl_Position = varyColor;
      //varyColor = (vec4(vColor, 1.0) + 1)/2;
      vary_uv= vUV; //permet UV et détails
	vary_normal = transpose(uInvModel3x3) * vNormal;

	vary_world_position = uModel * vec4(vPosition, 1.0);
	vary_world_position = vary_world_position / vary_world_position.w; //Normalization from w
}