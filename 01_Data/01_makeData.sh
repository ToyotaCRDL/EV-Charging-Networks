#!/bin/sh

makeData(){
    Tmp=tmp4makeData
    Out=org
    
    mkdir -p ${Tmp}
    mkdir -p ${Out}
    

    #-[Model parameter]--------------------------------------------------------------------------------------------
    # //                    argv[1]                 // Output Directory
    # int INI_NODE_NUM_A  = atoi(argv[2]);          // Initial number of nodes on Layer A
    # int INI_NODE_NUM_B  = atoi(argv[3]);          // Initial number of nodes on Layer B
    # double LOWER_DIST_A = atof(argv[4]);          // Lower bound of distance between ij on Layer A
    # double UPPER_DIST_A = atof(argv[5]);          // Upper bound of distance between ij on Layer A
    # double LOWER_DIST_B = atof(argv[6]);          // Lower bound of distance between ij on Layer B
    # double UPPER_DIST_B = atof(argv[7]);          // Upper bound of distance between ij on Layer B
    # double AREA_SIZE    = atoi(argv[8]);          // Area size 
    #--------------------------------------------------------------------------------------------------------------
    ## Compile
    gcc-10 -DDSFMT_MEXP=19937 -o ${Tmp}/tmp.out src/dSFMT.c src/geometric_multiplex.c -lm -g -O0 -Wall

    INI_NODE_NUM_A=${1}
    INI_NODE_NUM_B=${2}
    LOWER_DIST_A=0.00
    UPPER_DIST_A=${3}
    LOWER_DIST_B=`echo "${UPPER_DIST_A} + 0.000001" | bc`
    UPPER_DIST_B=${4}
    AREA_SIZE=${5}
    
    ## Execution
    cnt=1
    while [ "${cnt}" != "101" ]; do
	#              1      2                 3                 4               5               6               7               8            9
	${Tmp}/tmp.out ${Tmp} ${INI_NODE_NUM_A} ${INI_NODE_NUM_B} ${LOWER_DIST_A} ${UPPER_DIST_A} ${LOWER_DIST_B} ${UPPER_DIST_B} ${AREA_SIZE} ${cnt}
	
	## Output
	# network
	echo "i,j,lat.i,lon.i,lat.j,lon.j,distance" > ${Tmp}/header.txt
	files=${Tmp}/*_network_*.csv
	for f in ${files}; do
	    fname=`basename ${f} .csv`
	    
	    cat ${Tmp}/header.txt ${f} |
	    mchkcsv |
	    msortf f=i,j |
	    muniq -q k=i,j o=${Out}/${fname}_${cnt}.csv
	done
	
	rm ${Tmp}/*_network_*.csv
	cnt=`expr ${cnt} + 1`
    done
    
    
    rm -rf ${Tmp}
}

#-----------------------------------------------------------------------------
# Main
#-----------------------------------------------------------------------------
main(){
    #        1     2    3    4      5
    makeData "150" "30" ${1} "0.19" "1"
}

for THIS_DIST in 0.06 0.07 0.08 0.09 0.10 0.11 0.12 0.13 0.14 0.15 0.16 0.17 0.18 0.19 0.20 0.21 ; do
    main ${THIS_DIST} 
done
