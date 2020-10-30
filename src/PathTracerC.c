#include <stdlib.h> // card > pixar.ppm
#include <stdio.h>
#include <math.h>
#include <string.h> /* memset */
#include <unistd.h> /* close */

const int HIT_NONE = 0;
const int HIT_LETTER = 1;
const int HIT_WALL = 2;
const int HIT_SUN = 3;
const float RAND_MAX_FLOAT = (float)RAND_MAX;

typedef struct {
    float x, y, z;
} Vec;

Vec vec(float x, float y, float z) {
    Vec res = {.x = x, .y = y, .z = z};
    return res;
}

Vec plus(Vec a, Vec b) {
    return vec(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec minus(Vec a, Vec b) {
    return vec(a.x + b.x, a.y + b.y, a.z + b.z);
}

float length(Vec a) {
    return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
}

Vec factor(Vec a, float f) {
    return vec(a.x*f, a.y*f, a.z*f);
}

Vec normalize(Vec a) {    
    float len = length(a);
    return len > 0.0 ? factor(a, 1.0/len) : a;
}

float dotProduct(Vec a, Vec b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

float min(float l, float r) { return l < r ? l : r; }

float randomVal() { return (float) rand() / RAND_MAX_FLOAT; }

// Rectangle CSG equation. Returns minimum signed distance from
// space carved by
// lowerLeft vertex and opposite rectangle vertex upperRight.
// Negative value means we are inside the box.
float boxTest(Vec position, Vec lowerLeft, Vec upperRight) {
    Vec a = minus(position, lowerLeft);
    Vec b = minus(upperRight, position);
    return -min(min(
                    min(a.x, b.x),
                    min(a.y, b.y)
                ),
                min(a.z, b.z));
}

// Sample the world using Signed Distance Fields.
float queryDatabase(Vec position, int *hitType) {
  float distance = 1e9;
 
  *hitType = HIT_LETTER;

  float roomDist = 0.0;
  roomDist = min(
                -min(// Lower room
                     boxTest(position, vec(-30, -.5, -30), vec(30, 18, 30)),
                     // Upper room
                     boxTest(position, vec(-25, 17, -25), vec(25, 20, 25))
                ),
                boxTest( // Ceiling "planks" spaced 8 units apart.
                  vec(fmodf(fabsf(position.x), 8),
                      position.y,
                      position.z),
                  vec(1.5, 18.5, -25),
                  vec(6.5, 20, 25)
                )
  );
  //printf("queryDatabase distance %d", roomDist);
  if (roomDist < distance) {
      distance = roomDist;
      *hitType = HIT_WALL;
  }

  float sun = 1.9 - position.y; // Everything above 19.9 is light source.
  if (sun < distance) {
      distance = sun;
      *hitType = HIT_SUN;
  }

  return distance;
}


/// Returns hitType 0, 1, 2, or 3 and updates hit position/normal
int rayMarching(Vec position, Vec direction, Vec *hitPos, Vec *hitNorm) {
    int hitType = HIT_NONE;
    int noHitCount = 0;
    float d; // distance from closest object in world.
    static Vec v0 = {.x = 0.01, .y = 0.0, .z = 0.0};
    static Vec v1 = {.x = 0.0, .y = 0.01, .z = 0.0};
    static Vec v2 = {.x = 0.0, .y = 0.0, .z = 0.01};

    // Signed distance marching
    for (float totalD = 0.0; totalD < 100; totalD += d) {
        *hitPos = plus(position, factor(direction, totalD));
        d = queryDatabase(*hitPos, &hitType);
        printf("d = %.6f\n", d);
        if (d < .01 || ++noHitCount > 99) {
            *hitNorm = normalize(vec(queryDatabase(plus(*hitPos, v0), &noHitCount) - d,
                                    queryDatabase(plus(*hitPos, v1), &noHitCount) - d,
                                    queryDatabase(plus(*hitPos, v2), &noHitCount) - d));
            if (hitType == HIT_SUN)             printf("hitType = HIT SUN");
            return hitType;
        }
    }
    return 0;
}

void dbg(Vec a) {
    printf("dbg %.6f %.6f %.6f\n", a.x, a.y, a.z);
}

Vec trace(Vec position, Vec direction) {   
    Vec sampledPosition = vec(0, 0, 0);
    Vec normal = vec(0, 0, 0);
    Vec color = vec(0, 0, 0);
    int attenuation = 1;
    static const Vec lightDirection = {.x = 0.458, .y = 0.458, .z = 0.7624}; // Directional light
    static const Vec otherColor = {.x = 50.0, .y = 400.0, .z = 100.0};
    static const Vec sunColor = {.x = 50.0, .y = 80.0, .z = 100.0};
    //dbg(lightDirection);
    for (int bounceCount = 3; bounceCount--;) {
        int hitType = rayMarching(position, direction, &sampledPosition, &normal);

        if (hitType == HIT_NONE) break; // No hit. This is over, return color.
        if (hitType == HIT_LETTER) { // Specular bounce on a letter. No color acc.
            direction = plus(direction, factor(normal, dotProduct(normal, direction) * -2));
            position = plus(sampledPosition, factor(direction, 0.1));
            attenuation *= 0.2; // Attenuation via distance traveled.
        } else if (hitType == HIT_WALL) { // Wall hit uses color yellow?
            float incidence = dotProduct(normal, lightDirection);
            float p = 6.283185 * randomVal();
            float c = randomVal();
            float s = sqrtf(1 - c);
            float g = normal.z < 0 ? -1 : 1;
            float u = -1 / (g + normal.z);
            float v = normal.x * normal.y * u;
            Vec vec0 = factor(vec(v,
                                  g + normal.y * normal.y * u,
                                  -normal.y), cosf(p) * s);
            Vec vec1 = factor(vec(1 + g * normal.x * normal.x * u,
                                  g * v,
                                  -g * normal.x), sinf(p) * s);
            direction = plus(plus(vec0, vec1), factor(normal, sqrtf(c)));
                        
            position = plus(sampledPosition, factor(direction, 0.1));
            attenuation *= 0.2;
            if (incidence > 0 &&
                rayMarching(plus(sampledPosition, factor(normal, 0.1)),
                            lightDirection,
                            &sampledPosition,
                            &normal) == HIT_SUN)
              color = plus(color, factor(otherColor, attenuation*incidence));
        } else if (hitType == HIT_SUN) { //
            color = plus(color, factor(sunColor, attenuation)); 
            break; // Sun Color
        }
    }
    return color;
}

float reinhard(float a) {
    return 255.0*(a + 14.0/241)/(a + 255.0/241);
}

void createBMP(unsigned char data[], int w, int h, char* fName) {
    FILE *f;
    unsigned char *img = NULL;
    int filesize = 54 + 3*w*h;  //w is your image width, h is image height, both int

    img = (unsigned char *)malloc(3*w*h);
    memset(img, 0, 3*w*h);

    for(int i = 0; i < w; ++i) {
        for(int j = 0; j < h; ++j) {
            int indSource = (j*w + i)*3;
            int indTarget = (w*(h - 1 - j) + i)*3;

            img[indTarget + 2] = data[indSource    ];
            img[indTarget + 1] = data[indSource + 1];
            img[indTarget    ] = data[indSource + 2];
        }
    }

    unsigned char bmpfileheader[14] = { 'B', 'M', 0,0,0,0, 0,0, 0,0, 54, 0, 0,  0};
    unsigned char bmpinfoheader[40] = { 40,0,0,0, 0,0,0,0, 0,0, 0,0, 1,  0, 24, 0};
    unsigned char bmppad[3] = {0, 0, 0};

    bmpfileheader[ 2] = (unsigned char)(filesize    );
    bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
    bmpfileheader[ 4] = (unsigned char)(filesize>>16);
    bmpfileheader[ 5] = (unsigned char)(filesize>>24);

    bmpinfoheader[ 4] = (unsigned char)(w    );
    bmpinfoheader[ 5] = (unsigned char)(w>> 8);
    bmpinfoheader[ 6] = (unsigned char)(w>>16);
    bmpinfoheader[ 7] = (unsigned char)(w>>24);
    bmpinfoheader[ 8] = (unsigned char)(h    );
    bmpinfoheader[ 9] = (unsigned char)(h>> 8);
    bmpinfoheader[10] = (unsigned char)(h>>16);
    bmpinfoheader[11] = (unsigned char)(h>>24);

    f = fopen(fName,"wb");
    fwrite(bmpfileheader, 1, 14, f);
    fwrite(bmpinfoheader, 1, 40, f);
    int lenPad = (4 - (w*3)%4)%4;
    if (lenPad > 0) {
        for(int i = 0; i < h; i++) {
            fwrite(img + (w*i*3), 3, w, f);
            fwrite(bmppad, 1, lenPad, f);
        }
    } else {
        for(int i = 0; i < h; i++) {
            fwrite(img + (w*i*3), 3, w, f);
        }
    }   

    free(img);
    fclose(f);
}


int main() {
    unsigned char testData[27] = {255, 0, 0, 255, 255, 255, 255, 0, 0, 
                                    0, 0, 0, 255, 0, 0, 0, 0, 0, 
                                  255, 0, 0, 0, 0, 0, 255, 0, 0};
    createBMP(testData, 3, 3, "test.bmp");
    return 0;
  /*
    printf("hello world\n");
    Vec x = vec(3.0, 4.0, 5.0);
    printf("x-value is %d\n", (int)x.x);
    printf("length is %d\n", (int)length(vec(3.0, 4.0, 5.0)));
    getchar();
    */
    //return 0;

    const int w = 96, h = 54, samplesCount = 16;
    const Vec position = vec(-22, 5, 25);
    Vec goal = normalize(minus(vec(-3, 4, 0), position));
    Vec dirLeft = factor(normalize(vec(goal.z, 0, -goal.x)), 1.0/w);
    Vec dirUp0 = vec(goal.y*dirLeft.z - goal.z*dirLeft.y, goal.z*dirLeft.x - goal.x*dirLeft.z, goal.x*dirLeft.y - goal.y*dirLeft.x);
    Vec dirUp = factor(dirUp0, (float)h/(float)w);
   
    printf("P6 %d %d 255 ", w, h);
  
    for (int y = h; y--;)
        for (int x = w; x--;) {
            Vec color = vec(0, 0, 0);
            for (int p = samplesCount; --p;) {
                Vec a = plus(factor(dirLeft, (x - w/2) * randomVal()), factor(dirUp, (y - h/2) * randomVal()));             
                Vec b = normalize(plus(goal, a));
                dbg(b);
                Vec diff = trace(position, b);
                color = plus(color, diff);
                //dbg(diff);
            }

            color = vec(reinhard(color.x), reinhard(color.y), reinhard(color.z));
            //printf("%c%c%c", (int)color.x, (int)color.y, (int)color.z);
      }  
}

/*
  Vec f = position; // Flattened position (z=0)
  f.z = 0;
  char letters[15*4+1] =               // 15 two points lines
          "5O5_" "5W9W" "5_9_"         // P (without curve)
          "AOEO" "COC_" "A_E_"         // I
          "IOQ_" "I_QO"                // X
          "UOY_" "Y_]O" "WW[W"         // A
          "aOa_" "aWeW" "a_e_" "cWiO"; // R (without curve)

  for (int i = 0; i < sizeof(letters); i += 4) {
    Vec begin = Vec(letters[i] - 79, letters[i + 1] - 79) * .5;
    Vec e = Vec(letters[i + 2] - 79, letters[i + 3] - 79) * .5 + begin * -1;
    Vec o = f + (begin + e * min(-min((begin + f * -1) % e / (e % e),
                                      0),
                                 1)
                ) * -1;
    distance = min(distance, o % o); // compare squared distance.
  }
  distance = sqrtf(distance); // Get real distance, not square distance.

  // Two curves (for P and R in PixaR) with hard-coded locations.
  Vec curves[] = {Vec(-11, 6), Vec(11, 6)};
  for (int i = 2; i--;) {
    Vec o = f + curves[i] * -1;
    distance = min(distance,
                   o.x > 0 ? fabsf(sqrtf(o % o) - 2)
                           : (o.y += o.y > 0 ? -2 : 2, sqrtf(o % o))
               );
  }
  distance = powf(powf(distance, 8) + powf(position.z, 8), .125) - .5;
*/