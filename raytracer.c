struct rt_vec3
{
    f64 x;
    f64 y;
    f64 z;
};

struct rt_ray
{
    rt_vec3 origin;
    rt_vec3 dir;
};

struct rt_sphere
{
    rt_vec3 pos;
    f64 radius;
};

struct rt_camera
{
    rt_vec3 pos;
    rt_vec3 dir;
    rt_vec3 up;
    f64 half_fov_y_deg;
};

struct rt_mat3
{
    rt_vec3 i;
    rt_vec3 j;
    rt_vec3 k;
};

// tan, sqrt are supplied by the host

f64 to_radians(f64 degrees)
{
    return degrees / 360.0 * 2.0 * 3.14159265359;
}

f64 length(rt_vec3 vec)
{
    return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

rt_vec3 normalize(rt_vec3 vec)
{
    f64 len = length(vec);
    vec.x /= len;
    vec.y /= len;
    vec.z /= len;
    return vec;
}

rt_vec3 cross(rt_vec3 lhs, rt_vec3 rhs)
{
    rt_vec3 vec;
    vec.x = lhs.y * rhs.z - lhs.z * rhs.y;
    vec.y = lhs.z * rhs.x - lhs.x * rhs.z;
    vec.z = lhs.x * rhs.y - lhs.y * rhs.x;
    return vec;
}

rt_vec3 mul_vec3_scalar(rt_vec3 vec, f64 scalar)
{
    vec.x *= scalar;
    vec.y *= scalar;
    vec.z *= scalar;
    return vec;
}

rt_vec3 add_vec3(rt_vec3 lhs, rt_vec3 rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    return lhs;
}

rt_vec3 sub_vec3(rt_vec3 lhs, rt_vec3 rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    lhs.z -= rhs.z;
    return lhs;
}

rt_vec3 mul_vec3(rt_vec3 lhs, rt_vec3 rhs)
{
    lhs.x *= rhs.x;
    lhs.y *= rhs.y;
    lhs.z *= rhs.z;
    return lhs;
}

rt_vec3 mul_mat_vec3(rt_mat3 mat, rt_vec3 vec)
{
    mat.i = mul_vec3_scalar(mat.i, vec.x);
    mat.j = mul_vec3_scalar(mat.j, vec.y);
    mat.k = mul_vec3_scalar(mat.k, vec.z);
    return add_vec3(add_vec3(mat.i, mat.j), mat.k);
}

f64 dot(rt_vec3 lhs, rt_vec3 rhs)
{
    lhs = mul_vec3(lhs, rhs);
    return lhs.x + lhs.y + lhs.z;
}

bool collides(rt_ray ray, rt_sphere sphere)
{
    rt_vec3 sphere_to_ray = sub_vec3(ray.origin, sphere.pos);
    f64 a = dot(ray.dir, ray.dir);
    f64 b = 2.0 * dot(sphere_to_ray, ray.dir);
    f64 c = dot(sphere_to_ray, sphere_to_ray) - sphere.radius * sphere.radius;
    f64 discriminant = b * b - 4.0 * a * c;
    
    if (discriminant > 0.0)
        return true;

    return false;
}

void main()
{
    s32 width = 50;
    s32 height = 50;
    u8* image_buf = (u8*)malloc(width * height * 3);

    rt_camera camera;

    camera.pos.x = 0;
    camera.pos.y = 2;
    camera.pos.z = 3;

    camera.dir.x = -camera.pos.x;
    camera.dir.y = -camera.pos.y;
    camera.dir.z = -camera.pos.z;
    camera.dir = normalize(camera.dir);

    camera.up.x = 0;
    camera.up.y = 1;
    camera.up.z = 0;

    camera.half_fov_y_deg = 45;

    s32 num_spheres = 3;
    rt_sphere* spheres = (rt_sphere*)malloc(sizeof(rt_sphere) * num_spheres);

    spheres[0].radius = 1.5;
    spheres[0].pos.x = 0;
    spheres[0].pos.y = 1;
    spheres[0].pos.z = -3;

    spheres[1].radius = 1;
    spheres[1].pos.x = -4;
    spheres[1].pos.y = 1.1;
    spheres[1].pos.z = -2;

    spheres[2].radius = 1;
    spheres[2].pos.x = 3.5;
    spheres[2].pos.y = 1.1;
    spheres[2].pos.z = -2;

    f64 aspect_ratio = (f64)width / height;

    for(s32 y = 0; y < height; ++y)
    {
        for(s32 x = 0; x < width; ++x)
        {
            // in camera space
            rt_vec3 sensor_pos;
            sensor_pos.x = ( ((f64)x / width) * 2.0 - 1.0) * aspect_ratio;
            sensor_pos.y = -1.0 * ( ((f64)y / height) * 2.0 - 1.0);
            sensor_pos.z = 1.0 / tan(to_radians(camera.half_fov_y_deg));

            // camera to world space
            rt_mat3 mat;
            mat.i = normalize(cross(camera.dir, camera.up));
            mat.j = cross(mat.i, camera.dir);
            mat.k = camera.dir;

            rt_ray ray;
            ray.origin = camera.pos;
            ray.dir = normalize(mul_mat_vec3(mat, sensor_pos));

            bool hit = false;
            for(s32 i = 0; i < num_spheres; ++i)
            {
                if(collides(ray, spheres[i]))
                {
                    hit = true;
                    break;
                }
            }
            
            s32 idx = (y * width + x) * 3;

            if(hit)
            {
                image_buf[idx] = 255;
                image_buf[idx + 1] = 0;
                image_buf[idx + 2] = 0;
            }
            else
            {
                image_buf[idx] = 0;
                image_buf[idx + 1] = 0;
                image_buf[idx + 2] = 0;
            }
        }
    }

    write_ppm6("render_raytracer.ppm", width, height, image_buf);
}
