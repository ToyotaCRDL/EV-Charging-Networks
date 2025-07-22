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
    # int DIST_TYPE       = atoi(argv[7]);          // Distribution type of charging slot (norm. = 1, expo. = 2)
    # double DIST_PARA    = atof(argv[8]);          // 形状パラメータ
    # int SERVICE         = atoi(argv[9]);          // Maximum service rate
    # int STEP_NUM        = atoi(argv[10]);         // Number of steps for a simulation
    # int INI_AGENT_NUM   = atoi(argv[11]);         // 初期状態におけるエージェントの総数
    # int SEED            = atoi(argv[12]);         // 乱数のシード
    #--------------------------------------------------------------------------------------------------------------
    MAX_RESOURCE=1000000000
    GAMMA=1.000
    
    # 充電ステーションの配置数に関する分布: 指数分布
    DIST_TYPE=2

    if [ "${INI_AGENT_NUM}" = "10" ]; then
	LOWER=1
	UPPER=51
	BIN=1
    elif [ "${INI_AGENT_NUM}" = "50" ]; then
	LOWER=1
	UPPER=205
	BIN=5
    elif [ "${INI_AGENT_NUM}" = "100" ]; then
	LOWER=1
	UPPER=310
	BIN=10
    elif [ "${INI_AGENT_NUM}" = "1000" ]; then
	LOWER=1
	UPPER=2100
	BIN=50
    fi
    
    cnt=1
    while [ "${cnt}" != "101" ]; do
	
	LAMBDA=${LOWER}
	while [ `echo "${LAMBDA} > ${UPPER}" | bc` = "0" ]; do
	    echo "d: ${UPPER_DIST}, g:${GAMMA}, l:1/${LAMBDA}, agent: ${INI_AGENT_NUM}"
	    
	    PAIR=${STEP_NUM}_${INI_AGENT_NUM}_${MAX_RESOURCE}_${MAX_SOC}_${LAMBDA}_${SERVICE}_${GAMMA}_${cnt}
	    echo ${PAIR}
	    
	    gcc -DDSFMT_MEXP=19937 -o ${Tmp}/tmp_${PAIR}.out src/dSFMT.c src/congestion_simulation.c -lm -g -O0 -Wall
	    
	    if [ ! -e ${Out}/infra_step_${PAIR}.csv ]; then
		## Execution
		#                      1      2                      3                         4               5        6          7           8          9          10          11               12
		${Tmp}/tmp_${PAIR}.out ${Tmp} ${Org}/node_${cnt}.csv ${Org}/network_${cnt}.csv ${MAX_RESOURCE} ${GAMMA} ${MAX_SOC} ${DIST_TYPE} ${LAMBDA} ${SERVICE} ${STEP_NUM} ${INI_AGENT_NUM} ${cnt} &
	    fi
	    
	    LAMBDA=`echo "scale=1; ${LAMBDA} + ${BIN}" | bc`
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
	    
            rm -rf ${Tmp}/tmp_*.out*
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
for THIS_DIST in 0.06 0.11 0.16 0.21 ; do
    #    1            2
    main ${THIS_DIST} "10"
    main ${THIS_DIST} "50"
    main ${THIS_DIST} "100"
    main ${THIS_DIST} "1000"
done
