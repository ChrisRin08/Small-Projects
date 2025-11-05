#include "bmp.hpp"
#include <iostream>
#include <time.h>
#include <algorithm>
using namespace std;

void draw_rectangle(BMP &bmp,int x, int y, int l, int h, int b, color c ){
    for(int i=0;i<=l;i++){
        for(int ii=0;ii<=b;ii++){
        bmp.set_pixel(i+x, y+ii, c.r,c.g,c.b);
        bmp.set_pixel(i+x, y+h-ii, c.r, c.g,c.b);
        }
    }
    for(int j=0;j<=h;j++){
        for(int ii=0;ii<=b;ii++){
            bmp.set_pixel(x+ii, y+j, c.r,c.g,c.b);
            bmp.set_pixel(x+l-ii, y+j,c.r, c.g,c.b);
        }
    }
}
void draw_line(BMP &bmp, int x1, int y1, int x2, int y2, int b, color c) {

    
    if (x1 == x2) {
        if (y1 > y2) std::swap(y1, y2);

        for (int y = y1; y <= y2; y++) {
            bmp.set_pixel(x1, y, c.r, c.g, c.b);
        }
        return;
    }

    
    double slope = (double)(y2 - y1) / (double)(x2 - x1);

    
    if (x1 > x2) {
        swap(x1, x2);
        swap(y1, y2);
    }

    double y = (double)y1;
    for (int x = x1; x <= x2; x++) {
        for (int ii = 0; ii <= b; ii++) {
            bmp.set_pixel(x, (int)(y + ii), c.r, c.g, c.b);  
            bmp.set_pixel(x, (int)(y - ii), c.r, c.g, c.b);
        }
        y += slope;
    }
}


void draw_triangle(BMP &bmp,int x1, int y1, int x2, int y2, int x3, int y3, int b, color c ){

    draw_line(bmp, x1, y1, x2, y2,b, c);
    draw_line(bmp, x2, y2, x3, y3,b,  c);
    draw_line(bmp, x3, y3, x1, y1,b, c);
}




int main() {
    
    BMP bmp(500, 500); 
     srand(static_cast<unsigned int>(time(0)));
    color c(0,255,0);

    for(int i=0;i<5;i++){
        uint8_t x=rand()%500;
        uint8_t y=rand()%500;
        uint8_t l=rand()%500;
        uint8_t h=rand()%500;
        
        draw_rectangle(bmp,x,y,l,h,1,c);

    }

    for(int i=0;i<3;i++){
        uint8_t x1=rand()%500;
        uint8_t y1=rand()%500;
        uint8_t x2=rand()%500;
        uint8_t y2=rand()%500;
        uint8_t x3=rand()%500;
        uint8_t y3=rand()%500;
        
        draw_triangle(bmp,x1,y1,x2, y2,x3,y3, 3, c);

    }

    bmp.write("triangle_thick.bmp");

    //bmp.write("rect.bmp"); // Save the image to a file

    return 0;
}