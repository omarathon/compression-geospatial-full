import csv
import os
import sys

def create_slurm_script(index, file, initial_transformation, access_transformation, initial_codecs, access_codecs, output_dir):
    slurm_script_template = f"""#!/bin/bash
#SBATCH -J brr_access{index}
#SBATCH -A REDACTED
#SBATCH -p cclake-himem
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --mem=5000
#SBATCH --time=11:00:00
#SBATCH --mail-type=ALL

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

# Run the command
source ./modules.sh
CMD="./bench_pipeline '{file}' '256' '2000' '10' '{initial_codecs}' '{access_codecs}' 'morton' '{initial_transformation}' 'linear' '{access_transformation}'"
echo "Executing command: $CMD"
eval $CMD
"""

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    filename = f'slurm_script_{index}.slurm'
    script_path = os.path.join(output_dir, filename)

    with open(script_path, 'w') as script_file:
        script_file.write(slurm_script_template)

    return script_path

def create_slurm_scripts_from_csv(csv_file_path, output_dir):
    tiffs = ["../../../../tiffs/geotiffs/2656.tif",
            "../../../../tiffs/geotiffs/accessibility.tif",
            "../../../../tiffs/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif",
            "../../../../tiffs/geotiffs/slope-srtm_35_11.tif",
            "../../../../tiffs/geotiffs/srtm_45_15.tif"]

    initial_transformations = ["none", "threshold", "smoothAndShift", "valueBasedClassification", "valueShift"]
    access_transformations =  ["linearSum", "randomSum", "threshold", "smoothAndShift", "valueBasedClassification", "valueShift"]

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

                initial_codecs=None
                access_codecs=None
                with open(csv_file_path, mode='r', encoding='utf-8-sig') as csvfile:
                    reader = csv.DictReader(csvfile)
                    for row in reader:
                        file = row['File']
                        transformation = row['Transformation']
                        pareto_decompression = convert_to_bar_separated(row['Pareto Decompression'])
                        pareto_compression = convert_to_bar_separated(row['Pareto Compression'])

                        if tiff == file:
                            if transformation == initial_transformation:
                                initial_codecs = pareto_decompression
                                if access_transformation in ["linearSum", "randomSum"]: # no data change
                                    access_codecs = pareto_compression
                            if transformation == access_transformation:
                                access_codecs = pareto_compression

                if initial_codecs is None or len(initial_codecs) == 0:
                    sys.exit("no matching initial_codecs")
                if access_codecs is None or len(access_codecs) == 0:
                    sys.exit("no matching access_codecs")

                if "FastPFor_JustCopy" not in initial_codecs.split('|'):
                    initial_codecs += "|FastPFor_JustCopy"
                if "custom_direct_access" not in initial_codecs.split('|'):
                    initial_codecs += "|custom_direct_access"
                if "FastPFor_JustCopy" not in access_codecs.split('|'):
                    access_codecs += "|FastPFor_JustCopy"
                if "custom_direct_access" not in access_codecs.split('|'):
                    access_codecs += "|custom_direct_access"

                pareto_codecs = union_of_bar_separated_strings(initial_codecs, access_codecs)
                
                create_slurm_script(i, tiff, initial_transformation, access_transformation, pareto_codecs, pareto_codecs, output_dir)

                i+=1

if len(sys.argv) != 3:
    print("Usage: python script.py <path_to_csv_file> <output_directory>")
    sys.exit(1)

input_csv_file_path = sys.argv[1]
output_directory = sys.argv[2]

create_slurm_scripts_from_csv(input_csv_file_path, output_directory)