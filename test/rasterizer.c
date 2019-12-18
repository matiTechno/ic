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
    return 0;
}
