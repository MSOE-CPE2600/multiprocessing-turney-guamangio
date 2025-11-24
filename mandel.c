/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include "jpegrw.h"

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max );
static void show_help();

//thread changes (global variable)
int numThreads = 1;

//The struct for the threads because it can only take in 1 argument
typedef struct {
    imgRawImage *img;
    int startRow;
    int endRow;
    double xmin;
    double xmax;
    double ymin;
    double ymax;
    int max;
	int width;
	int height;
} ThreadArgs;

//added function for threading 
void *thread_worker(void* arg){

	ThreadArgs *t = (ThreadArgs*)arg;

	// For every pixel in the image...
	for(int j = t->startRow; j <= t->endRow; j++) {
		for(int i = 0 ; i < t->width; i++) {

			// Determine the point in x,y space for that pixel.
			//using the struct to direct all the information from the struct tot the variable
			double x = t->xmin + i * (t -> xmax- t->xmin)/ t->width;
			double y = t->ymin + j * (t -> ymax- t->ymin)/ t->height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,t->max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(t->img,i,j,iteration_to_color(iters, t-> max));
		}
	}
	return NULL;
}


int main( int argc, char *argv[] )
{
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.
	const char *outfile = "mandel.jpg";
	double xcenter = 0;
	double ycenter = 0;
	double xscale = 4;
	double yscale = 0; // calc later
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;

	//multiprocessor change
	int numProcs = 1;
	int movieMode = 0;
	int totalFrames = 50;


	// For each command line argument given,
	// override the appropriate configuration value.

	//The 'n' with M is telling the function that M doesn't have an arguement o it won't eat the other arguements. 
	while((c = getopt(argc,argv,"Mn:n:t:x:y:s:W:H:m:o:h"))!=-1) {
		switch(c) 
		{
			case 'M':
				movieMode = 1;
				break;
			case 'n':
				numProcs = atoi(optarg);
				break;
			case 't':
				numThreads = atoi(optarg);
				break;
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	//This is the movie mode. (It's just generated 50 images).
    if (movieMode) {

        printf("Movie mode: generating %d frames using %d processes\n",
                totalFrames, numProcs);

		//assigns the amount of frames per each child.
        int framesPerChild = totalFrames / numProcs;
		//will see how much frames are left over to give to a child to work on.
        int leftover = totalFrames % numProcs;
		//array of pids made for the amount of children going to be made within the program.
        pid_t pids[numProcs];

        for (int i = 0; i < numProcs; i++) {

            int start = i * framesPerChild;
            int count = framesPerChild;

			//checking for leftover images 
            if (i == numProcs - 1){
                count += leftover;
			}

			//creates a child
            pid_t pid = fork();


			//checks if creating a child caused an error.
            if (pid < 0) {
                perror("fork");
                exit(1);
            }

			//what the child does.
            if (pid == 0) {
				// This loop is make sure that all the children don't run into each other.
                for (int f = start; f < start + count; f++) {

                    char outname[64];
					// print the string and create the output name. 
                    sprintf(outname, "mandel%d.jpg", f);

					//changes the scale after each image for each child
                    double scale = xscale * (0.98 * f);
                    char scaleStr[32];
                    sprintf(scaleStr, "%lf", scale);

					//makes sure to set the x string and y string to make sure they aren't x everytime a child calls the program.
					char xStr[32];
					char yStr[32];
					sprintf(xStr, "%lf", xcenter);
					sprintf(yStr, "%lf", ycenter);

					//changes max iterations if you want
					char mIter[32];
					sprintf(mIter, "%d", max);

					//have the child run an instance of the program without hitting movie mode
					//to not accidently run movie mode again infinitely.
                    execl("./mandel", "mandel",
                          "-x", xStr,
                          "-y", yStr,
                          "-s", scaleStr,
                          "-o", outname,
						  "-m", mIter,
                          NULL);

                    perror("execl failed");
                    exit(1);
                }
                exit(0);
            }

			//places the pid of each child into this array
            pids[i] = pid;
        }

		//This is where the parent will stay until it's children has finished making the movie.
        for (int i = 0; i < numProcs; i++)
			//array is used to see if each child has finished their image.
            waitpid(pids[i], NULL, 0);

        printf("Movie finished.\n");
        return 0;
    }

	//nothing changes below here as this is what the child will do for each image.

	// Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
	yscale = xscale / image_width * image_height;

	// Display the configuration of the image.
	printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",xcenter,ycenter,xscale,yscale,max,outfile);

	// Create a raw image of the appropriate size.
	imgRawImage* img = initRawImage(image_width,image_height);

	// Fill it with a black
	setImageCOLOR(img,0);

	// Compute the Mandelbrot image
	compute_image(img,xcenter-xscale/2,xcenter+xscale/2,ycenter-yscale/2,ycenter+yscale/2,max);

	// Save the image in the stated file.
	storeJpegImageFile(img,outfile);

	// free the mallocs
	freeRawImage(img);

	return 0;
}

/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point(double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while((x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}



/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max )
{

	int width = img->width;
	int height = img->height;

	//The multithreading changes
	int rowsPerThread =  height/numThreads;
	int leftover2 = height%numThreads;


	//threads created and the amount of them
	pthread_t threads[numThreads];
	ThreadArgs args[numThreads];

	int currentStart = 0;

	//places all the varibale that is used in this program into a struct to be used to make the image.
	for (int t = 0; t < numThreads; t++) {
        int thisRows = rowsPerThread;
        if (t == numThreads - 1){
            thisRows += leftover2;
		}

        args[t].img = img;

        args[t].xmin = xmin;
        args[t].xmax = xmax;
        args[t].ymin = ymin;
        args[t].ymax = ymax;

        args[t].max = max;
        args[t].width = width;
        args[t].height = height;

        args[t].startRow = currentStart;
        args[t].endRow   = currentStart + thisRows - 1;

        currentStart += thisRows;
    }


	//The start and the end of the threading 
	for(int i = 0; i < numThreads; i++){
		pthread_create(&threads[i], NULL, thread_worker, (void*) &args[i]);
	}
	for(int i = 0; i < numThreads; i++){
		pthread_join(threads[i],NULL);
	}

}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-M          This is what enables movie mode, and to not let the child accidently inifinite loop the code.\n");
	printf("-n <num>    This is the number of processors that you want to use to make the movie.\n");
	printf("-t <amo>    Thi is the number of threads that you want to use? (default=1) and (max amount = 20).\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}