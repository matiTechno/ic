void main()
{
    s32 width = 300;
    s32 height = 300;

    u8* image_buf = (u8*)malloc(3 * width * height);

    f64 left = -0.711580;
    f64 right = -0.711562;
    f64 top = 0.252133;
    f64 bottom = 0.252143;
    s32 iterations = 900;

    for(s32 j = 0; j < height; ++j)
    {
        for(s32 i = 0; i < width; ++i)
        {
            f64 xcoeff = (f64)i / (width - 1);
            f64 ycoeff = (f64)j / (height - 1);

            f64 x0 = (1 - xcoeff) * left + xcoeff * right;
            f64 y0 = (1 - ycoeff) * top  + ycoeff * bottom;

            s32 iteration = 0;
            f64 x = 0;
            f64 y = 0;

            while(x * x + y * y < 4 && iteration < iterations)
            {
                f64 x_temp = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = x_temp;
                ++iteration;
            }

            s32 idx = (j * width + i) * 3;
            u8 color = 255 * (f64)iteration / iterations + 0.5;
            image_buf[idx] = color;
            image_buf[idx + 1] = color;
            image_buf[idx + 2] = color;
        }
    }

    write_ppm6("render_fractal.ppm", width, height, image_buf);
}
