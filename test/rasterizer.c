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

    s32 int_digits = 0;
    s32 i = begin;

    for (; i < size; ++i)
    {
        if (str[i] == ' ' || str[i] == '.')
            break;
        ++int_digits;
    }

    s32 fraction_begin = i + 1;
    f32 number = 0;

    for (s32 i = begin; i < begin + int_digits; ++i)
        number += pow(10, int_digits - 1) * (str[i] - '0');

    for (s32 i = fraction_begin; i < size; ++i)
        number += pow(10, fraction_begin - i - 1) * (str[i] - '0');

    if (negate)
        number *= -1;

    return number;
}

s32 get_number_str_size(const u8* str)
{
    s32 size = 0;
    while (*str != ' ' && *str != '\n' && *str != '\r')
    {
        ++size;
        ++str;
    }
    return size;
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
    s32 idx_vnorm = 0;
    s32 idx_face = 0;

    for (s32 i = 0; i < obj_data_size; ++i)
    {
        if (obj_data[i] == ' ' || obj_data[i] == '\n' || obj_data[i] == '\r')
            continue;

        if (obj_data[i] == 'v' && obj_data[i + 1] == ' ')
        {
            i += 2;
            s32 number_str_size;

            number_str_size = get_number_str_size(obj_data + i);
            vertices[idx_vpos].pos.x = atof(obj_data + i, number_str_size);
            i += number_str_size + 1;

            number_str_size = get_number_str_size(obj_data + i);
            vertices[idx_vpos].pos.y = atof(obj_data + i, number_str_size);
            i += number_str_size + 1;

            number_str_size = get_number_str_size(obj_data + i);
            vertices[idx_vpos].pos.z = atof(obj_data + i, number_str_size);
            i += number_str_size;

            ++idx_vpos;
            continue;
        }

        if (obj_data[i] == 'v' && obj_data[i + 1] == 'n')
        {
            i += 3;
            s32 number_str_size;

            number_str_size = get_number_str_size(obj_data + i);
            vertices[idx_vnorm].normal.x = atof(obj_data + i, number_str_size);
            i += number_str_size + 1;

            number_str_size = get_number_str_size(obj_data + i);
            vertices[idx_vnorm].normal.y = atof(obj_data + i, number_str_size);
            i += number_str_size + 1;

            number_str_size = get_number_str_size(obj_data + i);
            vertices[idx_vnorm].normal.z = atof(obj_data + i, number_str_size);
            i += number_str_size;

            ++idx_vnorm;
            continue;
        }
    }
    return 0;
}
