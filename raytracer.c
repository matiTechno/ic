// tan, sqrt, random01 are supplied by the host
// todo; if possible pass structures by pointers to functions

struct rt_vec3
{
    f64 x;
    f64 y;
    f64 z;
};

struct rt_mat3
{
    rt_vec3 i;
    rt_vec3 j;
    rt_vec3 k;
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
    rt_vec3 world_up;
    f64 half_fov_y_deg;
};

// I miss unions and function pointers here...
struct rt_material
{
    s32 type; // 0 - lambertian, 1 - metal
    rt_vec3 albedo;
    f64 fuzziness;
};

f64 to_radians(f64 degrees)
{
    return degrees / 360.0 * 2.0 * 3.14159265359;
}

rt_vec3 add_vec3_scalar(rt_vec3 vec, f64 scalar)
{
    vec.x += scalar;
    vec.y += scalar;
    vec.z += scalar;
    return vec;
}

rt_vec3 mul_vec3_scalar(rt_vec3 vec, f64 scalar)
{
    vec.x *= scalar;
    vec.y *= scalar;
    vec.z *= scalar;
    return vec;
}

rt_vec3 div_vec3_scalar(rt_vec3 vec, f64 scalar)
{
    vec.x /= scalar;
    vec.y /= scalar;
    vec.z /= scalar;
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

rt_vec3 transform_vec3(rt_mat3 mat, rt_vec3 vec)
{
    mat.i = mul_vec3_scalar(mat.i, vec.x);
    mat.j = mul_vec3_scalar(mat.j, vec.y);
    mat.k = mul_vec3_scalar(mat.k, vec.z);
    return add_vec3(add_vec3(mat.i, mat.j), mat.k);
}

rt_vec3 cross(rt_vec3 lhs, rt_vec3 rhs)
{
    rt_vec3 vec;
    vec.x = lhs.y * rhs.z - lhs.z * rhs.y;
    vec.y = lhs.z * rhs.x - lhs.x * rhs.z;
    vec.z = lhs.x * rhs.y - lhs.y * rhs.x;
    return vec;
}

f64 dot(rt_vec3 lhs, rt_vec3 rhs)
{
    lhs = mul_vec3(lhs, rhs);
    return lhs.x + lhs.y + lhs.z;
}

f64 length(rt_vec3 vec)
{
    return sqrt(dot(vec, vec));
}

rt_vec3 normalize(rt_vec3 vec)
{
    f64 len = length(vec);
    return div_vec3_scalar(vec, len);
}

bool scatter_lambertian()
{

}

bool scatter_metal()
{

}

// returns 0 if there is no collision
f64 get_collision_distance(rt_ray ray, rt_sphere sphere, f64 min_distance, f64 max_distance)
{
    rt_vec3 sphere_to_ray = sub_vec3(ray.origin, sphere.pos);
    // quadratic equation
    f64 a = dot(ray.dir, ray.dir);
    f64 b = 2.0 * dot(sphere_to_ray, ray.dir);
    f64 c = dot(sphere_to_ray, sphere_to_ray) - sphere.radius * sphere.radius;
    f64 discriminant = b * b - 4.0 * a * c;
    
    if (discriminant > 0.0)
    {
        for (s32 i = 0; i < 2; ++i)
        {
            s32 sign = i * 2 - 1;
            f64 distance = (-b + sign * sqrt(discriminant)) / 2.0;

            if (distance >= min_distance && distance <= max_distance)
                return distance;
        }
    }

    return 0.0;
}

rt_vec3 get_ray_color(rt_ray ray, rt_sphere* spheres, s32 depth)
{
    f64 max_distance = 1000000.0; // todo; F64_MAX would be useful
    s32 sphere_idx = -1;

    for(s32 i = 0; i < num_spheres; ++i)
    {
        f64 distance = get_collision_distance(ray, spheres[i], 0.01, max_distance);

        if(distance)
        {
            max_distance = distance; // next collision can't be further away than the current one (depth test)
            sphere_idx = i;
        }
    }

    if (sphere_idx != -1)
    {
        rt_vec3 color;
        color.x = 1.0;
        color.y = 0.0;
        color.z = 0.0;
        return color;
    }

    // miss, render sky color (gradient)
    f64 t = (ray.dir.y + 1.0) / 2.0;

    rt_vec3 color_bot;
    color_bot.x = 0.037;
    color_bot.y = 0.0;
    color_bot.z = 0.486;

    rt_vec3 color_top;
    color_top.x = 1.0;
    color_top.y = 0.597;
    color_top.z = 0.0;

    return add_vec3(mul_vec3_scalar(color_bot, 1.0 - t), mul_vec3_scalar(color_top, t));
}

void main()
{
    s32 width = 300;
    s32 height = 300;
    s32 samples_per_pixel = 1;
    u8* image_buf = (u8*)malloc(width * height * 3);

    rt_camera camera;

    camera.pos.x = 0;
    camera.pos.y = 2;
    camera.pos.z = 3;

    // point at 0, 0, 0
    camera.dir.x = -camera.pos.x;
    camera.dir.y = -camera.pos.y;
    camera.dir.z = -camera.pos.z;
    camera.dir = normalize(camera.dir);

    camera.world_up.x = 0;
    camera.world_up.y = 1;
    camera.world_up.z = 0;

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

    // camera space to world space transformation matrix
    rt_mat3 mat;
    mat.i = normalize(cross(camera.dir, camera.world_up)); // right
    mat.j = cross(mat.i, camera.dir); // up
    mat.k = camera.dir; // forward

    rt_ray ray;
    ray.origin = camera.pos;

    rt_vec3 color;
    rt_vec3 sensor_pos;

    for(s32 y = 0; y < height; ++y)
    {
        for(s32 x = 0; x < width; ++x)
        {
            color.x = color.y = color.z = 0;

            for (s32 i = 0; i < samples_per_pixel; ++i)
            {
                f64 pix_x = x + random01();
                f64 pix_y = y + random01();

                // in camera space
                sensor_pos.x = (pix_x / width * 2.0 - 1.0) * aspect_ratio;
                sensor_pos.y = (pix_y / height * 2.0 - 1.0) * -1.0; // negated because viewport Y grows down and camera Y grows up
                sensor_pos.z = 1.0 / tan(to_radians(camera.half_fov_y_deg));

                ray.dir = normalize(transform_vec3(mat, sensor_pos));
                color = add_vec3(color, get_ray_color(ray, spheres, 0));
            }

            color = div_vec3_scalar(color, samples_per_pixel); // average
            // convert to u8 representation
            color = mul_vec3_scalar(color, 255.0);
            color = add_vec3_scalar(color, 0.5);

            s32 buf_idx = (y * width + x) * 3;

            image_buf[buf_idx] = color.x;
            image_buf[buf_idx + 1] = color.y;
            image_buf[buf_idx + 2] = color.z;
        }
    }

    write_ppm6("render_raytracer.ppm", width, height, image_buf);
}
