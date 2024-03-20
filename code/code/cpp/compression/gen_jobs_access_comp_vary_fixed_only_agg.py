import csv
import os
import sys

def create_slurm_script(index, file, initial_transformation, access_transformation, initial_codecs, access_codecs, output_dir):
    # Define the template of the Slurm script with placeholders
    slurm_script_template = f"""#!/bin/bash
#SBATCH -J brr_access{index}
#SBATCH -A MADHAVAPEDDY-SL3-CPU
#SBATCH -p cclake-himem
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --mem=5000
#SBATCH --time=11:00:00
#SBATCH --mail-type=ALL
#SBATCH --array=0-2 # Adjust this based on the number of jobs

numnodes=$SLURM_JOB_NUM_NODES
numtasks=$SLURM_NTASKS
mpi_tasks_per_node=$(echo "$SLURM_TASKS_PER_NODE" | sed -e  's/^\\([0-9][0-9]*\).*$/\\1/')

# Load modules and environment
. /etc/profile.d/modules.sh
module purge
module load rhel7/default-ccl

workdir="$SLURM_SUBMIT_DIR"
export OMP_NUM_THREADS=1
np=$[${{numnodes}}*${{mpi_tasks_per_node}}]
export I_MPI_PIN_DOMAIN=omp:compact
export I_MPI_PIN_ORDER=scatter

cd $workdir
echo -e "Changed directory to `pwd`.\\n"

JOBID=$SLURM_JOB_ID

echo -e "JobID: $JOBID\\n======"
echo "Time: `date`"
echo "Running on master node: `hostname`"
echo "Current directory: `pwd`"

if [ "$SLURM_JOB_NODELIST" ]; then
        #! Create a machine file:
        export NODEFILE=`generate_pbs_nodefile`
        cat $NODEFILE | uniq > machine.file.$JOBID
        echo -e "\\nNodes allocated:\\n================"
        echo `cat machine.file.$JOBID | sed -e 's/\\..*$//g'`
fi

echo -e "\\nnumtasks=$numtasks, numnodes=$numnodes, mpi_tasks_per_node=$mpi_tasks_per_node (OMP_NUM_THREADS=$OMP_NUM_THREADS)"

numBlocks=( "500" "1000" "2000" )
numBlock=${{numBlocks[$SLURM_ARRAY_TASK_ID]}}

# Run the command
source ./modules.sh
CMD="./access_comp_vary_change_dict '{file}' '256' '$numblock' '10' '{initial_codecs}' '{access_codecs}' 'morton' '{initial_transformation}' 'linear' '{access_transformation}'"
echo "Executing command: $CMD"
eval $CMD
"""

    # Make sure the output directory exists
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Define the filename for the Slurm script
    filename = f'slurm_script_{index}.slurm'
    script_path = os.path.join(output_dir, filename)

    # Save the generated script to a file
    with open(script_path, 'w') as script_file:
        script_file.write(slurm_script_template)

    return script_path

def create_slurm_scripts_from_csv(csv_file_path, output_dir):
    # Read the CSV file and iterate over each row to generate Slurm scripts
    tiffs = ["../../../../tiffs/geotiffs/2656.tif",
            "../../../../tiffs/geotiffs/accessibility.tif",
            "../../../../tiffs/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif",
            "../../../../tiffs/geotiffs/slope-srtm_35_11.tif",
            "../../../../tiffs/geotiffs/srtm_45_15.tif"]

    initial_transformations = ["none", "threshold", "smoothAndShift", "valueBasedClassification", "valueShift"]
    access_transformations =  ["linearSum", "randomSum"]

    def convert_to_bar_separated(input_string):
        codecs = [codec.strip('"') for codec in input_string.split()]
        return '|'.join(codecs)

    def union_of_bar_separated_strings(str1, str2):
        list1 = str1.split('|')
        list2 = str2.split('|')
        union_set = set(list1) | set(list2)
        result = '|'.join(union_set)
        return result

    i = 0
    for tiff in tiffs:
        for initial_transformation in initial_transformations:
            for access_transformation in access_transformations:
                # find initial codecs and access codecs from csv
                initial_codecs=""
                initial_codecs2=""
                access_codec="FastPFor_JustCopy" # dummy
                with open(csv_file_path, mode='r', encoding='utf-8-sig') as csvfile:
                    reader = csv.DictReader(csvfile)
                    for row in reader:
                        file = row['File']
                        transformation = row['Transformation']
                        pareto_decompression = convert_to_bar_separated(row['Pareto Decompression'])
                        pareto_compression = convert_to_bar_separated(row['Pareto Compression'])
                        pareto_compression_decompression = convert_to_bar_separated(row['Pareto Compression Decompression'])

                        if tiff == file:
                            if transformation == initial_transformation:
                                initial_codecs = pareto_decompression
                                initial_codecs2 = pareto_compression_decompression
                                # if access_transformation in ["linearSum", "randomSum"]: # no data change
                                #     access_codecs = pareto_compression
                            # if transformation == access_transformation:
                            #     access_codecs = pareto_compression

                # if initial_codecs is None or len(initial_codecs) == 0 or len(initial_codecs2) == 0:
                #     sys.exit("no matching initial_codecs")
                # if access_codecs is None or len(access_codecs) == 0:
                #     sys.exit("no matching access_codecs")

                # skip if not an optimised summing codec
                
                initial_codecs_arr = []
                for initial_codec in initial_codecs.split('|'):
                    if initial_codec.startswith('[+]_custom_rle_vecavx512'):
                        initial_codecs_arr.append(initial_codec)
                for initial_codec in initial_codecs2.split('|'):
                    if initial_codec.startswith('[+]_custom_rle_vecavx512') and (initial_codec not in initial_codecs_arr): 
                        initial_codecs_arr.append(initial_codec)

                initial_codecs_arr.append('simdcomp')
                initial_codecs_arr.append('FastPFor_SIMDPFor+VariableByte')
                initial_codecs_arr.append('FastPFor_JustCopy')
                initial_codecs_arr.append('custom_direct_access')

                print(len(initial_codecs_arr))



                # add mantatory codecs
                # if "FastPFor_JustCopy" not in initial_codecs.split('|'):
                #     initial_codecs += "|FastPFor_JustCopy"
                # if "custom_direct_access" not in initial_codecs.split('|'):
                #     initial_codecs += "|custom_direct_access"
                # if "FastPFor_JustCopy" not in access_codecs.split('|'):
                #     access_codecs += "|FastPFor_JustCopy"
                # if "custom_direct_access" not in access_codecs.split('|'):
                #     access_codecs += "|custom_direct_access"
                
                create_slurm_script(i, tiff, initial_transformation, access_transformation, '|'.join(initial_codecs_arr), access_codec, output_dir)

                i+=1

# Check if the correct number of arguments are provided
if len(sys.argv) != 3:
    print("Usage: python script.py <path_to_csv_file> <output_directory>")
    sys.exit(1)

# Get the CSV file path and output directory from the command-line arguments
input_csv_file_path = sys.argv[1]
output_directory = sys.argv[2]

# Generate the Slurm scripts
create_slurm_scripts_from_csv(input_csv_file_path, output_directory)