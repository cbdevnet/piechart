#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "svg_header.h"

typedef struct /*_2D_COORDS*/ {
	int x;
	int y;
} COORDS;

typedef enum {
	IGNORE,
	LEGEND,
	COLOR,
	VALUE,
	EXPLODE
} SLICE_PROP;

typedef struct /*_PIE_SLICE*/ {
	char* legend_text;	//slice description
	char* color;		//color override
	double absolute;	//absolute slice value from input
	double relative;	//calculated relative slice size
	COORDS slice;		//
	COORDS offset;
	COORDS legend;
	int bigarc_flag;
	bool anchor_start;
} PIE_SLICE;

struct /*_PIE_CONFIG*/ {
	COORDS origin;
	int radius;
	FILE* input_handle;
	size_t num_props;
	SLICE_PROP* props;

	char* delimiter;
	char* default_fill;
	char* border_color;
	bool print_legend;
	int explode_offset;

	size_t num_slices;
	PIE_SLICE* slices;
} PIECHART = {
	.origin = {
		.x = 350,
		.y = 300
	},
	.radius = 250,
	.num_props = 1,
	.props = NULL,

	.delimiter = ",",
	.default_fill = "white",
	.border_color = "black",
	.print_legend = true,
	.explode_offset = 0,

	.num_slices = 0,
	.slices = NULL
};

int usage(char* fn){
	printf("piechart - Creates SVG pie charts\n");
	printf("piechart reads data to be plotted from stdin and outputs the resulting plot to stdout\n");
	printf("Usage: %s [--delimiter <delim>] [--order <order-spec>] [--color <random | contrast | color-spec>] [--border <color-spec>] [--explode <offset>] [--no-legend] [inputfile]\n", fn);
	return 1;
}

int args_parse(int argc, char** argv){
	int i;
	char* token = NULL;
	char* input_file = NULL;
	char* input_order = strdup("value,color,legend"); //cant have this as constant string because we're calling strtok

	//parse raw arguments
	for(i = 0; i < argc; i++){
		if(!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")){
			free(input_order);
			return -1;
		}
		if(!strcmp(argv[i], "--delimiter")){
			PIECHART.delimiter = argv[++i];
		}
		else if(!strcmp(argv[i], "--order")){
			if(argv[i + 1]){
				free(input_order);
				input_order = strdup(argv[++i]);
			}
			else{
				break;
			}
		}
		else if(!strcmp(argv[i], "--border")){
			PIECHART.border_color = argv[++i];
		}
		else if(!strcmp(argv[i], "--color")){
			PIECHART.default_fill = argv[++i];
		}
		else if(!strcmp(argv[i], "--explode")){
			if(argv[i + 1]){
				PIECHART.explode_offset = strtoul(argv[++i], NULL, 10);
			}
			else{
				break;
			}
		}
		else if(!strcmp(argv[i], "--no-legend")){
			PIECHART.print_legend = false;
		}
		else{
			input_file = argv[i];
		}
	}

	//ensure arguments matched parameters
	if(i - argc > 0){
		fprintf(stderr, "Option expects a parameter: %s\n", argv[argc - 1]);
		free(input_order);
		return -1;
	}

	if(i - argc < 0){
		fprintf(stderr, "Argument parsing stopped at parameter: %s\n", argv[i]);
		free(input_order);
		return -1;
	}

	//sanity check
	if(strlen(input_order) < 4){
		fprintf(stderr, "Invalid property order\n");
		free(input_order);
		return -1;
	}

	//count props
	for(i = 0; input_order[i]; i++){
		if(input_order[i] == ','){
			PIECHART.num_props++;
		}
	}

	PIECHART.props = calloc(PIECHART.num_props, sizeof(SLICE_PROP));
	if(!PIECHART.props){
		free(input_order);
		fprintf(stderr, "Failed to allocate memory\n");
		return -1;
	}

	//parse prop order
	i = 0;
	do {
		token = strtok(i ? NULL:input_order, ",");
		if(token){
			if(!strcmp(token, "ignore")){
				PIECHART.props[i] = IGNORE;
			}
			else if(!strcmp(token, "legend")){
				PIECHART.props[i] = LEGEND;
			}
			else if(!strcmp(token, "color")){
				PIECHART.props[i] = COLOR;
			}
			else if(!strcmp(token, "value")){
				PIECHART.props[i] = VALUE;
			}
			else if(!strcmp(token, "explode")){
				PIECHART.props[i] = EXPLODE;
			}
			else{
				fprintf(stderr, "No such property: %s\n", token);
				free(input_order);
				return -1;
			}
		}
		i++;
	} while(token);

	//clean up
	free(input_order);

	//open input file if given
	if(input_file){
		PIECHART.input_handle = fopen(input_file, "r");
	}
	if(!PIECHART.input_handle){
		fprintf(stderr, "Failed to open input file %s\n", input_file);
		return -1;
	}
	return 0;
}

void global_cleanup(){
	size_t u;
	if(PIECHART.props){
		free(PIECHART.props);
	}

	if(PIECHART.input_handle){
		fclose(PIECHART.input_handle);
	}

	if(PIECHART.slices){
		for(u = 0; u < PIECHART.num_slices; u++){
			free(PIECHART.slices[u].color);
			free(PIECHART.slices[u].legend_text);
		}
		free(PIECHART.slices);
	}
}

char* generate_color(char* mode, double current_angle){
	char* color = calloc(8, sizeof(char));
	static double base_hue;
	static int current_contrast;
	double current_hue;
	unsigned red = 0, green = 0, blue = 0;

	if(!color){
		return NULL;
	}

	if(!strcmp(mode, "random")){
		red = rand() % 255;
		green = rand() % 255;
		blue = rand() % 255;
	}

	if(!strcmp(mode, "contrast")){
		current_contrast = current_contrast ? 0:1;
		mode = "hsv";
	}

	if(!strcmp(mode, "hsv")){
		double saturation = 1.0;
		double value = 1.0;

		//first-time initialization
		base_hue = (base_hue) ? base_hue:(rand() % 359) + 1;
		current_hue = fmod((base_hue + current_angle + 180.0 * current_contrast), 360.0);

		//convert to rgb
		double chroma = value * saturation * 255;
		red = green = blue = value * 255 - chroma;

		current_hue /= 60;
		double x = chroma * (1 - fabs(fmod(current_hue, 2.0) - 1.0));

		switch((int)floor(current_hue)){
			case 0:
			case 1:
			case 6:
				red += (floor(current_hue) == 1.0) ? x : chroma;
				green += (floor(current_hue) != 1.0) ? x : chroma;
				break;
			case 2:
			case 3:
				green += (floor(current_hue) == 3.0) ? x : chroma;
				blue += (floor(current_hue) != 3.0) ? x : chroma;
				break;
			case 4:
			case 5:
				red += (floor(current_hue) == 4.0) ? x : chroma;
				blue += (floor(current_hue) != 4.0) ? x : chroma;
				break;
			default:
				free(color);
				return NULL;
		}
	}

	//limit values
	red = (red > 0xFF) ? 0xFF:red;
	green = (green > 0xFF) ? 0xFF:green;
	blue = (blue > 0xFF) ? 0xFF:blue;
	snprintf(color, 8, "#%02X%02X%02X", red, green, blue);
	return color;
}

int gather_data(){
	char* line_buffer = NULL, *token = NULL;
	size_t line_buffer_length = 0;
	ssize_t bytes_read = 0, i;

	//read lines into slices
	do {
		bytes_read = getline(&line_buffer, &line_buffer_length, PIECHART.input_handle);
		//kill the newline
		for(i = bytes_read - 1; i >= 0 && isspace(line_buffer[i]); i--){
			line_buffer[i] = 0;
		}

		if(line_buffer[0] == '#'){
			continue;
		}

		if(bytes_read > 0){
			//fill the slice with data
			PIE_SLICE current = {
				.legend_text = NULL,
				.color = NULL,
				.absolute = 0,
				.slice = {
					PIECHART.radius,
					PIECHART.radius
				},
				.offset = {
					PIECHART.explode_offset,
					PIECHART.explode_offset
				},
				.legend = {
					PIECHART.radius + 5,
					PIECHART.radius + 5
				},
				.anchor_start = true
			};

			//read properties
			i = 0;
			do {
				token = strtok(i ? NULL:line_buffer, PIECHART.delimiter);
				if(token){
					if(i >= PIECHART.num_props){
						break;
					}

					switch(PIECHART.props[i]){
						case IGNORE:
							break;
						case LEGEND:
							if(!current.legend_text && strlen(token) > 0){
								current.legend_text = strdup(token);
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
								if(current.absolute < 0){
									fprintf(stderr, "Piecharts with negative absolute values are not supported\n");
									free(line_buffer);
									return -1;
								}
							}
							break;
						case EXPLODE:
							current.offset.x = strtoul(token, NULL, 10);
							current.offset.y = strtoul(token, NULL, 10);
							break;
					}
				}
				i++;
			} while(token);

			//push the slice into the main array
			PIECHART.num_slices++;
			PIECHART.slices = realloc(PIECHART.slices, PIECHART.num_slices * sizeof(PIE_SLICE));
			if(!PIECHART.slices){
				fprintf(stderr, "Failed to allocate memory for pie slice\n");
				return -1;
			}
			PIECHART.slices[PIECHART.num_slices - 1] = current;
		}
	} while(bytes_read >= 0);
	
	free(line_buffer);
	return 0;
}

int calculate_slices(){
	double slice_sum = 0;
	double current_angle = 0;
	int offset_angle;
	ssize_t u;

	//calculate relative values and coordinates as well as randomized colors
	for(u = 0; u < PIECHART.num_slices; u++){
		slice_sum += PIECHART.slices[u].absolute;
	}
	for(u = 0; u < PIECHART.num_slices; u++){
		PIECHART.slices[u].relative = PIECHART.slices[u].absolute / slice_sum;
		offset_angle = current_angle + PIECHART.slices[u].relative * 180;
		PIECHART.slices[u].anchor_start = offset_angle > 180;
		current_angle += PIECHART.slices[u].relative * 360;
		PIECHART.slices[u].bigarc_flag = ((PIECHART.slices[u].relative * 360) > 180) ? 1:0;
		PIECHART.slices[u].slice.x *= sin(current_angle * M_PI / 180);
		PIECHART.slices[u].slice.y *= -cos(current_angle * M_PI / 180);
		PIECHART.slices[u].offset.x *= sin(offset_angle * M_PI / 180);
		PIECHART.slices[u].offset.y *= -cos(offset_angle * M_PI / 180);
		PIECHART.slices[u].legend.x *= sin(offset_angle * M_PI / 180);
		PIECHART.slices[u].legend.y *= -cos(offset_angle * M_PI / 180);

		//generate random color if requested
		if(!PIECHART.slices[u].color && (!strcmp(PIECHART.default_fill, "random") 
					|| !strcmp(PIECHART.default_fill, "hsv")
					|| !strcmp(PIECHART.default_fill, "contrast"))){
			PIECHART.slices[u].color = generate_color(PIECHART.default_fill, current_angle);
		}
	}
	return 0;
}

int print_svg(){
	size_t u;
	//print svg header
	fwrite(svg_header, 1, svg_header_len, stdout);

	//debug print data
	//for(i = 0; i < num_slices; i++){
	//	fprintf(stderr, "%d: end_x=%d, end_y=%d, arc=%d, abs=%f, rel=%f, col=%s, leg=%s\n", i, slices[i].slice_x, slices[i].slice_y, slices[i].bigarc_flag, slices[i].absolute, slices[i].relative, slices[i].color, slices[i].legend);
	//}

	//print svg data
	for(u = 0; u < PIECHART.num_slices; u++){
		int last_slice = (u ? u - 1:PIECHART.num_slices - 1);

		fprintf(stdout, "<path d=\""); //begin path
		fprintf(stdout, "M%d,%d ", PIECHART.origin.x, PIECHART.origin.y); //move to origin
		fprintf(stdout, "m%d,%d ", PIECHART.slices[u].offset.x, PIECHART.slices[u].offset.y); //explode
		fprintf(stdout, "l%d,%d ", PIECHART.slices[last_slice].slice.x, PIECHART.slices[last_slice].slice.y); //line to last slice end
		fprintf(stdout, "a%d,%d 0 %d,1 %d,%d ",
				PIECHART.radius,
				PIECHART.radius,
				PIECHART.slices[u].bigarc_flag,
				PIECHART.slices[u].slice.x - PIECHART.slices[last_slice].slice.x,
				PIECHART.slices[u].slice.y - PIECHART.slices[last_slice].slice.y);
		//fprintf(stdout, "l%d,%d ", slices[i].slice_x - slices[last_slice].slice_x, slices[i].slice_y - slices[last_slice].slice_y);
		fprintf(stdout, "z\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1\" stroke-linejoin=\"round\" />\n",
				PIECHART.slices[u].color ? PIECHART.slices[u].color:PIECHART.default_fill, 
				PIECHART.border_color);

		if(PIECHART.print_legend && PIECHART.slices[u].legend_text){
			fprintf(stdout, "<text text-anchor=\"%s\" x=\"%d\" y=\"%d\">%s</text>\n",
					PIECHART.slices[u].anchor_start ? "end":"start",
					PIECHART.slices[u].legend.x + PIECHART.slices[u].offset.x + PIECHART.origin.x,
					PIECHART.slices[u].legend.y + PIECHART.slices[u].offset.y + PIECHART.origin.y,
					PIECHART.slices[u].legend_text);
		}
	}

	fputs("</svg>", stdout);
	return 0;
}

int main(int argc, char** argv){
	//initialize in non-static context
	PIECHART.input_handle = stdin;

	//parse command line arguments
	if(args_parse(argc - 1, argv + 1) < 0){
		global_cleanup();
		exit(usage(argv[0]));
	}

	if(gather_data() < 0 || PIECHART.num_slices < 1){
		fprintf(stderr, "Invalid data read\n");
		global_cleanup();
		exit(usage(argv[0]));
	}

	if(calculate_slices() < 0){
		fprintf(stderr, "Failed to calculate slice extents\n");
		global_cleanup();
		exit(usage(argv[0]));
	}

	if(print_svg() < 0){
		fprintf(stderr, "Failed to write SVG\n");
		global_cleanup();
		exit(usage(argv[0]));
	}

	global_cleanup();
	return 0;
}
