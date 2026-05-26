#version 330 core
layout (location = 0) in vec2 aPos;  // 0~1 정규화 좌표

uniform vec2 uOffset;  // NDC 기준 좌하단 시작점
uniform vec2 uSize;    // NDC 기준 너비/높이

void main()
{
    vec2 pos    = uOffset + aPos * uSize;
    gl_Position = vec4(pos, 0.0, 1.0);
}
