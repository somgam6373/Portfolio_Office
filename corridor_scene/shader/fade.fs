#version 330 core
out vec4 FragColor;

uniform float alpha;  // 0.0 = transparent, 1.0 = full black

void main()
{
    FragColor = vec4(0.0, 0.0, 0.0, alpha);
}
