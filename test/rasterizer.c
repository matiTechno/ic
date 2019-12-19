struct vec3_t
{
    f32 x;
    f32 y;
    f32 z;
};

struct vertex_t
{
    vec3_t pos;
    vec3_t normal;
};

struct face_t
{
    s32 v1;
    s32 v2;
    s32 v3;
};

f32 atof(const u8* str, s32 size)
{
    bool negate = false;
    s32 begin = 0;

    if (str[0] == '-')
    {
        negate = true;
        begin = 1;
    }

    f32 exponent = 0;
    s32 i = begin;

    for (; i < size; ++i)
    {
        if (str[i] == ' ' || str[i] == '.')
            break;
        if (!exponent)
            exponent = 1;
        else
            exponent *= 10;
    }

    s32 fraction_begin = i + 1;
    f32 number = 0;

    for (s32 i = begin; exponent >= 1; exponent /= 10)
    {
        number += exponent * (str[i] - '0');
        ++i;
    }

    for (s32 i = fraction_begin; i < size; ++i)
    {
        number += exponent * (str[i] - '0');
        exponent /= 10;
    }

    if (negate)
        number *= -1;
    return number;
}

s32 get_number_str_size(const u8* str)
{
    const u8* it = str;

    if (*it == '-')
        ++it;

    while (*it >= '0' && *it <= '9')
        ++it;

    if (*it == '.')
        ++it;
    else
        return it - str;

    while (*it >= '0' && *it <= '9')
        ++it;

    return it - str;
}

s32 main()
{
    u8* obj_data;
    s32 obj_data_size;
    read_file("test/model.obj", &obj_data, &obj_data_size);
    s32 vertices_size = 0;
    s32 faces_size = 0;

    for (s32 i = 0; i < obj_data_size; ++i)
    {
        if (obj_data[i] == 'v' && obj_data[i + 1] == ' ')
            ++vertices_size;
        else if (obj_data[i] == 'f')
            ++faces_size;
    }

    prints("vertices");
    printf(vertices_size);
    prints("faces");
    printf(faces_size);

    vertex_t* vertices = (vertex_t*)malloc(vertices_size * sizeof(vertex_t));
    face_t* faces = (face_t*)malloc(faces_size * sizeof(face_t));
    s32 idx_vpos = 0;
    s32 idx_vnormal = 0;
    s32 idx_face = 0;

    for (s32 i = 0; i < obj_data_size; ++i)
    {
        if (obj_data[i] == 'v' && obj_data[i + 1] == ' ')
        {
            i += 2;
            for (s32 coord = 0; coord < 3; ++coord)
            {
                s32 number_str_size = get_number_str_size(obj_data + i);
                *(&vertices[idx_vpos].pos.x + coord) = atof(obj_data + i, number_str_size);
                i += number_str_size + 1;
            }
            i -= 1;
            ++idx_vpos;
            continue;
        }

        if (obj_data[i] == 'v' && obj_data[i + 1] == 'n')
        {
            i += 3;
            for (s32 coord = 0; coord < 3; ++coord)
            {
                s32 number_str_size = get_number_str_size(obj_data + i);
                *(&vertices[idx_vnormal].normal.x + coord) = atof(obj_data + i, number_str_size);
                i += number_str_size + 1;
            }
            i -= 1;
            ++idx_vnormal;
            continue;
        }

        if (obj_data[i] == 'f')
        {
            i += 2;
            for (s32 vid = 0; vid < 3; ++vid)
            {
                s32 number_str_size = get_number_str_size(obj_data + i);
                *(&faces[idx_face].v1 + vid) = atof(obj_data + i, number_str_size) - 1; // in obj vertices start from index 1, we have to correct this

                while (obj_data[i] != ' ' && obj_data[i] != '\n' && i < obj_data_size) // (i < obj_data_size), in case a file doesn't end with a newline
                    ++i;
                ++i;
            }
            i -= 1;
            ++idx_face;
            continue;
        }
    }
    s32 img_width = 1000;
    s32 img_height = 1000;
    u8* img_buf = (u8*)malloc(img_width * img_height * 3);
    f32* depth_buf = (f32*)malloc(img_width * img_height * sizeof(f32));

    // clear buffers
    for (s32 i = 0; i < img_width * img_height * 3; ++i)
        img_buf[i] = 0;
    
    // model is facing positive z direction in left-handed system, depth test passes if a new depth value is greater than the previous one
    for (s32 i = 0; i < img_width * img_height; ++i)
        depth_buf[i] = -1000;

    vec3_t lpos;
    lpos.x = 50;
    lpos.y = 100;
    lpos.z = 100;

    // shine in the direction of a scene center
    vec3_t ldir;
    ldir.x = 0 - lpos.x;
    ldir.y = 0 - lpos.y;
    ldir.z = 0 - lpos.z;
    ldir = normalize(ldir);

    for (s32 i = 0; i < faces_size; ++i)
    {
        vertex_t v1 = vertices[faces[i].v1];
        vertex_t v2 = vertices[faces[i].v2];
        vertex_t v3 = vertices[faces[i].v3];

        // this invertes vertex pos.y and changes the winding order from CCW to CW
        map_to_image(&v1, img_width, img_height);
        map_to_image(&v2, img_width, img_height);
        map_to_image(&v3, img_width, img_height);

        f32 xmin = max(0, min(min(v1.pos.x, v2.pos.x), v3.pos.x));
        f32 xmax = min(img_width, max(max(v1.pos.x, v2.pos.x), v3.pos.x));
        f32 ymin = max(0, min(min(v1.pos.y, v2.pos.y), v3.pos.y));
        f32 ymax = min(img_height, max(max(v1.pos.y, v2.pos.y), v3.pos.y));

        f32 det = determinant(v1.pos.x, v1.pos.y, v2.pos.x, v2.pos.y, v3.pos.x, v3.pos.y);

        for (s32 y = ymin; y < ymax; ++y)
        {
            for (s32 x = xmin; x < xmax; ++x)
            {
                f32 det1 = determinant(v2.pos.x, v2.pos.y, v3.pos.x, v3.pos.y, x, y);
                f32 det2 = determinant(v3.pos.x, v3.pos.y, v1.pos.x, v1.pos.y, x, y);
                f32 det3 = determinant(v1.pos.x, v1.pos.y, v2.pos.x, v2.pos.y, x, y);

                if (det1 < 0 || det2 < 0 || det3 < 0)
                    continue;

                // barycentric coordinates
                f32 bc1 = det1 / det;
                f32 bc2 = det2 / det;
                f32 bc3 = det3 / det;

                f32 depth = bc1 * v1.pos.z + bc2 * v2.pos.z + bc3 * v3.pos.z;
                s32 px_idx = (s32)y * img_width + (s32)x;

                if (depth > depth_buf[px_idx])
                {
                    depth_buf[px_idx] = depth;
                    // phong shading

                    vec3_t normal = mul_vec3_scalar(v1.normal, bc1);
                    normal = add_vec3(normal, mul_vec3_scalar(v2.normal, bc2));
                    normal = add_vec3(normal, mul_vec3_scalar(v3.normal, bc3));
                    normal = normalize(normal);
                    
                    vec3_t neg_ldir = mul_vec3_scalar(ldir, -1);
                    f32 in = max(0, dot(neg_ldir, normal));

                    img_buf[px_idx * 3] = 255 * in;
                    img_buf[px_idx * 3 + 1] = 255 * in;
                    img_buf[px_idx * 3 + 2] = 255 * in;
                }
            }
        }
    }
    write_ppm6("render_triangles.ppm", img_width, img_height, img_buf);
    return 0;
}

vec3_t add_vec3(vec3_t lhs, vec3_t rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    return lhs;
}

vec3_t mul_vec3_scalar(vec3_t v, f32 s)
{
    v.x *= s;
    v.y *= s;
    v.z *= s;
    return v;
}

void map_to_image(vertex_t* v, s32 img_width, s32 img_height)
{
    v->pos.x = (v->pos.x + 1) / 2 * img_width;
    v->pos.y = (v->pos.y * -1 + 1) / 2 * img_height;
}

f32 min(f32 lhs, f32 rhs)
{
    if (lhs < rhs)
        return lhs;
    return rhs;
}

f32 max(f32 lhs, f32 rhs)
{
    if (lhs > rhs)
        return lhs;
    return rhs;
}

// returns a positive value for a clockwise order
f32 determinant(f32 ax, f32 ay, f32 bx, f32 by, f32 cx, f32 cy)
{
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

f32 dot(vec3_t lhs, vec3_t rhs)
{
    lhs.x *= rhs.x;
    lhs.y *= rhs.y;
    lhs.z *= rhs.z;
    return lhs.x + lhs.y + lhs.z;
}

f32 length(vec3_t vec)
{
    return sqrt(dot(vec, vec));
}

vec3_t normalize(vec3_t vec)
{
    f32 len = length(vec);
    vec.x /= len;
    vec.y /= len;
    vec.z /= len;
    return vec;
}
