#!/bin/sh

doSimulation(){
    INI_NODE_NUM=${1}
    UPPER_DIST=${2}
    SERVICE=${3}
    STEP_NUM=${4}
    INI_AGENT_NUM=${5}
    MAX_SOC=${6}
    AREA_SIZE=${7}
    
    Org=../../01_Data/out_${INI_NODE_NUM}_${UPPER_DIST}_${AREA_SIZE}
    Tmp=tmp4doSimulation
    Out=out_${INI_NODE_NUM}_${UPPER_DIST}_${AREA_SIZE}
    
    mkdir -p ${Tmp}
    mkdir -p ${Out}

    #-[Model parameter]--------------------------------------------------------------------------------------------
    # //                         argv[1]            // Output Directory
    # //                         argv[2]            // Input File (Nodes)
    # //                         argv[3]            // Input File (Network)
    # double MAX_RESOURCE = atof(argv[4]);          // B
    # double GAMMA        = atof(argv[5]);          // velocity on Layer2 (velocity = 1 on Layer1)
    # int MAX_SOC         = atoi(argv[6]);          // Maximum SoC
    # double MU           = atof(argv[7]);          // mu for normal dist. to choose number of charging station for each node
    # int SERVICE         = atoi(argv[8]);          // Maximum service rate
    # int STEP_NUM        = atoi(argv[9]);          // Number of steps for a simulation
    # int INI_AGENT_NUM   = atoi(argv[10]);         // 初期状態におけるエージェントの総数
    # int SEED            = atoi(argv[11]);         // 乱数のシード
    #--------------------------------------------------------------------------------------------------------------
    MAX_RESOURCE=1000000000
    GAMMA=1.000

    if [ "${INI_AGENT_NUM}" = "10" ]; then
	LOWER=0.0
	UPPER=10
	BIN=1
    elif [ "${INI_AGENT_NUM}" = "50" ]; then
	LOWER=0.0
	UPPER=10
	BIN=1
    elif [ "${INI_AGENT_NUM}" = "100" ]; then
	LOWER=0.0
	UPPER=30
	BIN=1
    elif [ "${INI_AGENT_NUM}" = "1000" ]; then
	LOWER=0.0
	UPPER=300
	BIN=10
    fi
    
    cnt=1
#    while [ "${cnt}" != "101" ]; do
    while [ "${cnt}" != "2" ]; do

        MU=${LOWER}
	while [ `echo "${MU} > ${UPPER}" | bc` = "0" ]; do
	    echo "g:${GAMMA}, mu:${MU}, agent: ${INI_AGENT_NUM}, dist: ${2}"

	    PAIR=${STEP_NUM}_${INI_AGENT_NUM}_${MAX_RESOURCE}_${MAX_SOC}_${MU}_${SERVICE}_${GAMMA}_${cnt}
	    echo ${PAIR}

	    gcc-10 -DDSFMT_MEXP=19937 -o ${Tmp}/tmp_${PAIR}.out src/dSFMT.c src/congestion_simulation_expo.c -lm -g -O0 -Wall

	    if [ ! -e ${Out}/infra_step_${PAIR}.csv ]; then
		## Execution
		#                      1      2                      3                         4               5        6          7     8          9           10               11
		${Tmp}/tmp_${PAIR}.out ${Tmp} ${Org}/node_${cnt}.csv ${Org}/network_${cnt}.csv ${MAX_RESOURCE} ${GAMMA} ${MAX_SOC} ${MU} ${SERVICE} ${STEP_NUM} ${INI_AGENT_NUM} ${cnt} &
	    fi

	    MU=`echo "scale=10; ${MU} + ${BIN}" | bc | sed 's/^\./0./'`
	done
	wait
	
	## Output
	if [ `ls -l ${Tmp}/agent_* | wc -l` != "0" ] ; then
	    echo "t,failure,success,charging,skip" > ${Tmp}/header.txt
	    files=${Tmp}/infra_step_*.csv
	    for f in ${files}; do
		fname=`basename ${f} .csv`

		cat ${Tmp}/header.txt ${f} |
		mchkcsv o=${Out}/${fname}_${cnt}.csv
	    done

	    rm ${Tmp}/*
	fi

	cnt=`expr ${cnt} + 1`
    done

    
    rm -rf ${Tmp}
}

#-----------------------------------------------------------------------------
# Main
#-----------------------------------------------------------------------------
main(){
    #            - network - service steps  agents maxSoC area_size
    #            1     2     3       4      5      6      7
    doSimulation "150" ${1}  "640"   "1000" ${2}   "10"   "1"
}

for THIS_DIST in 0.06 0.07 0.08 0.09 0.10 0.11 0.12 0.13 0.14 0.15 0.16 0.17 0.18 0.19 0.20 0.21 ; do
    #    1            2
#    main ${THIS_DIST} "10"
#    main ${THIS_DIST} "50"
#    main ${THIS_DIST} "100"
    main ${THIS_DIST} "1000"
done
