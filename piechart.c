#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include <math.h>
#include "svg_header.h"

typedef struct /*_PIE_SLICE*/ {
	char* legend;
	char* color;
	double absolute;
	double relative;
	int slice_x;
	int slice_y;
	int bigarc_flag;
	int offset_x;
	int offset_y;
} PIE_SLICE;

typedef enum {
	IGNORE,
	LEGEND,
	COLOR,
	VALUE,
	EXPLODE
} SLICE_PROP;

int usage(char* fn){
	printf("piechart - Creates SVG pie charts\n");
	printf("piechart reads data to be plotted from stdin and outputs the resulting plot to stdout\n");
	printf("Usage: %s [--delimiter <delim>] [--order <order-spec>] [--color <random | color-spec>] [--border <color-spec>] [--explode <offset>] [--no-legend] [inputfile]\n", fn);
	return 1;
}

int main(int argc, char** argv){
	int i = 0;
	unsigned num_slices = 0;
	PIE_SLICE* slices = NULL;

	char* input_order = strdup("value,color,legend"); //cant have this as constant string because we're calling strtok
	char* token;
	unsigned num_props = 0;
	SLICE_PROP* props = NULL;

	char* input_file = NULL;
	FILE* input_handle = stdin;

	char* delimiter = ",";
	char* default_fill = "white";
	char* border_color = "black";
	bool print_legend = true;
	int explode_offset = 0;

	char* line_buffer = NULL;
	size_t line_buffer_length = 0;
	ssize_t bytes_read = 0;

	double slice_sum = 0;

	struct /*_PIE_CONFIG*/ {
		int origin_x;
		int origin_y;
		int radius;
	} PIE = {
		.origin_x = 200,
		.origin_y = 200,
		.radius = 150
	};

	int current_angle = 0;

	//parse arguments
	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "--delimiter")){
			if(i + 1 < argc){
				delimiter = argv[++i];
			}
			else{
				exit(usage(argv[0]));
			}
		}
		else if(!strcmp(argv[i], "--order")){
			if(i + 1 < argc){
				free(input_order);
				input_order = strdup(argv[++i]);
			}
			else{
				exit(usage(argv[0]));
			}
		}
		else if(!strcmp(argv[i], "--border")){
			if(i + 1 < argc){
				border_color = argv[++i];
			}
			else{
				exit(usage(argv[0]));
			}
		}
		else if(!strcmp(argv[i], "--color")){
			if(i + 1 < argc){
				default_fill = argv[++i];
			}
			else{
				exit(usage(argv[0]));
			}
		}
		else if(!strcmp(argv[i], "--explode")){
			if(i + 1 < argc){
				explode_offset = strtoul(argv[++i], NULL, 10);
			}
			else{
				exit(usage(argv[0]));
			}
		}
		else if(!strcmp(argv[i], "--no-legend")){
			print_legend = false;
		}
		else{
			input_file = argv[i];
		}
	}

	if(input_file){
		input_handle = fopen(input_file, "r");
	}

	//sanity check
	if(!input_handle){
		fprintf(stderr, "Failed to open input file %s\n", input_file);
		free(input_order);
		exit(usage(argv[0]));
	}

	if(strlen(input_order) < 4){
		free(input_order);
		fclose(input_handle);
		exit(usage(argv[0]));
	}
	
	//count props
	for(i = 0; input_order[i]; i++){
		if(input_order[i] == ','){
			num_props++;
		}
	}
	num_props += 1;

	props = realloc(props, num_props * sizeof(SLICE_PROP));
	if(!props){
		free(input_order);
		fclose(input_handle);
		fprintf(stderr, "Failed to allocate memory\n");
		exit(1);
	}

	//parse prop order
	i = 0;
	do {
		token = strtok(i ? NULL:input_order, ",");
		if(token){
			if(!strcmp(token, "ignore")){
				props[i] = IGNORE;
			}
			else if(!strcmp(token, "legend")){
				props[i] = LEGEND;
			}
			else if(!strcmp(token, "color")){
				props[i] = COLOR;
			}
			else if(!strcmp(token, "value")){
				props[i] = VALUE;
			}
			else if(!strcmp(token, "explode")){
				props[i] = EXPLODE;
			}
			else{
				fprintf(stderr, "No such property: %s\n", token);
				free(props);
				free(input_order);
				fclose(input_handle);
				exit(usage(argv[0]));
			}
		}
		i++;
	} while(token);

	//read lines into slices
	do {
		bytes_read = getline(&line_buffer, &line_buffer_length, input_handle);
		//kill the newline
		for(i = bytes_read - 1; i >= 0 && isspace(line_buffer[i]); i--){
			line_buffer[i] = 0;
		}

		if(bytes_read > 0){
			//fill the slice with data
			PIE_SLICE current = {
				.legend = NULL,
				.color = NULL,
				.absolute = 0,
				.offset_x = explode_offset,
				.offset_y = explode_offset
			};

			i = 0;
			do {
				token = strtok(i ? NULL:line_buffer, delimiter);
				if(token){
					if(i >= num_props){
						break;
					}

					switch(props[i]){
						case IGNORE:
							continue;
						case LEGEND:
							if(!current.legend){
								current.legend = strdup(token);
							}
							break;
						case COLOR:
							if(!current.color){
								current.color = strdup(token);
							}
							break;
						case VALUE:
							if(!current.absolute){
								current.absolute = strtod(token, NULL);
							}
							break;
						case EXPLODE:
							current.offset_x = strtoul(token, NULL, 10);
							current.offset_y = strtoul(token, NULL, 10);
							break;
					}
				}
				i++;
			} while(token);

			//generate random color if wanted
			if(!current.color && !strcmp(default_fill, "random")){
				current.color = calloc(8, sizeof(char));
				if(current.color){
					snprintf(current.color, 8, "#%02X%02X%02X", rand() % 255, rand() % 255, rand() % 255);
				}
			}

			//push the slice into the main array
			num_slices++;
			slices = realloc(slices, num_slices * sizeof(PIE_SLICE));
			if(!slices){
				fprintf(stderr, "Failed to allocate memory for pie slice\n");
				//this is kinda not really nice to look at, but hey.
				goto bail;
			}
			slices[num_slices - 1] = current;
		}
	} while(bytes_read >= 0);

	if(num_slices < 1){
		//not ideal, meh
		goto bail;
	}

	//calculate relative values and coordinates
	for(i = 0; i < num_slices; i++){
		slice_sum += slices[i].absolute;
	}

	for(i = 0; i < num_slices; i++){
		slices[i].relative = slices[i].absolute / slice_sum;
		int offset_angle = current_angle + slices[i].relative * 180;
		current_angle += slices[i].relative * 360;
		slices[i].bigarc_flag = ((slices[i].relative * 360) > 180) ? 1:0;
		slices[i].slice_x = sin(current_angle * M_PI / 180) * PIE.radius;
		slices[i].slice_y = -cos(current_angle * M_PI / 180) * PIE.radius;
		slices[i].offset_x *= sin(offset_angle * M_PI / 180);
		slices[i].offset_y *= -cos(offset_angle * M_PI / 180);
	}
	
	//print svg header
	fwrite(svg_header, 1, svg_header_len, stdout);
	
	//debug print data
	//for(i = 0; i < num_slices; i++){
	//	fprintf(stderr, "%d: end_x=%d, end_y=%d, arc=%d, abs=%f, rel=%f, col=%s, leg=%s\n", i, slices[i].slice_x, slices[i].slice_y, slices[i].bigarc_flag, slices[i].absolute, slices[i].relative, slices[i].color, slices[i].legend);
	//}

	//print svg data
	for(i = 0; i < num_slices; i++){
		int last_slice = (i ? i - 1:num_slices - 1);

		fprintf(stdout, "<path d=\""); //begin path
		fprintf(stdout, "M%d,%d ", PIE.origin_x, PIE.origin_y); //move to origin
		fprintf(stdout, "m%d,%d ", slices[i].offset_x, slices[i].offset_y); //explode
		fprintf(stdout, "l%d,%d ", slices[last_slice].slice_x, slices[last_slice].slice_y); //line to last slice end
		fprintf(stdout, "a%d,%d 0 %d,1 %d,%d ", PIE.radius, PIE.radius, slices[i].bigarc_flag, slices[i].slice_x - slices[last_slice].slice_x, slices[i].slice_y - slices[last_slice].slice_y);
		//fprintf(stdout, "l%d,%d ", slices[i].slice_x - slices[last_slice].slice_x, slices[i].slice_y - slices[last_slice].slice_y);
		fprintf(stdout, "z\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1\" stroke-linejoin=\"round\" />\n", slices[i].color ? slices[i].color:default_fill, border_color);
	}

	fputs("</svg>", stdout);

	bail:
	if(slices){
		for(i = 0; i < num_slices; i++){
			free(slices[i].color);
			free(slices[i].legend);
		}
	}

	free(line_buffer);
	free(input_order);
	free(slices);
	free(props);
	return 0;
}
