#!/bin/sh

remakeData(){
    INI_NODE_NUM=${1}
    LOWER_DIST=0.00
    UPPER_DIST=${2}
    AREA_SIZE=${3}
    
    Org=org
    Tmp=tmp4remakeData
    Out=out_${INI_NODE_NUM}_${UPPER_DIST}_${AREA_SIZE}
    
    mkdir -p ${Tmp}
    mkdir -p ${Out}
    
    
    files=${Org}/dense_network_N_${INI_NODE_NUM}_range_${LOWER_DIST}_${UPPER_DIST}_area_${AREA_SIZE}_*.csv
    for f in ${files}; do
	cnt=`basename ${f} .csv | awk -F"_" '{ print $10 }'`
	
        # 最大強連結成分のみを取り出す
	# i,j,lat.i,lon.i,lat.j,lon.j,distance
	mcut f=i,j i=${f} |
	msortf f=i,j |
	muniq -q k=i,j o=${Tmp}/xxa
	
	python src/get_lscc.py ${Tmp}/xxa ${Tmp}/xxlist
	
	mchkcsv i=${Tmp}/xxlist |
	msortf f=id o=${Tmp}/xxref
	
	# i,j,lat.i,lon.i,lat.j,lon.j,distance
	msortf f=i i=${f} |
	mcommon k=i K=id m=${Tmp}/xxref |
	msortf f=j |
        mcommon k=j K=id m=${Tmp}/xxref o=${Tmp}/xxpre

	# idの振り直し
	mcut f=i:id i=${Tmp}/xxpre o=${Tmp}/xxi
	mcut f=j:id i=${Tmp}/xxpre o=${Tmp}/xxj
	mcat i=${Tmp}/xxi,${Tmp}/xxj |
	msortf f=id |
	muniq -q k=id |
	msortf f=id%n |
	msetstr a=dammy v=hoge |
	mnumber -q k=dammy a=id2 S=0 |
	mcut f=id,id2 |
	msortf f=id o=${Tmp}/xxref2
	
	#
	msortf f=i i=${Tmp}/xxpre |
	mjoin k=i K=id f=id2 m=${Tmp}/xxref2 |
	mcut -r f=i |
	mfldname f=id2:i |
	msortf f=j |
	mjoin k=j K=id f=id2 m=${Tmp}/xxref2 |
	mcut -r f=j |
	mfldname f=id2:j |
	mcut f=i,j,distance o=${Tmp}/xxorg
	
	# nodes
	# i,j,lat.i,lon.i,lat.j,lon.j,distance
	for SIDE in i j ; do
	    mcut f=${SIDE}:id i=${Tmp}/xxorg |
	    msortf f=id |
	    mnumber -q k=id a=deg S=1 |
	    muniq -q k=id |
	    msetstr a=side v=${SIDE} o=${Tmp}/xxnode_${SIDE}
	done
	mcat i=${Tmp}/xxnode_'*' |
	msortf f=id,side |
	mcross -q k=id s=side f=deg |
	mfldname f=i:outdeg |
	mfldname f=j:indeg |
	mcut f=id,outdeg,indeg |
	muniq -q k=id |
	mnullto f=outdeg,indeg v=0 |
	mcal c='${outdeg}+${indeg}' a=deg |
	mcut f=id,outdeg,indeg,deg |
	msortf f=id o=${Tmp}/node.csv
	
	msortf f=id%n i=${Tmp}/node.csv |
	sed -e '1d' > ${Out}/node_${cnt}.csv
	
	# i,j,lat.i,lon.i,lat.j,lon.j,distance
	mfldname f=i:from i=${Tmp}/xxorg |
	mfldname f=j:to |
	mfldname f=to:i |
	mfldname f=from:j o=${Tmp}/xxrev
	
	mcat i=${Tmp}/xxorg,${Tmp}/xxrev |
	msortf f=i,j |
	muniq -q k=i,j |
	msetstr a=layer v=1 |
	mcut f=i,j,layer,distance |
	mdelnull f=i,j |
	msortf f=i%n,j%n |
	sed -e '1d' > ${Out}/network_${cnt}.csv
	
	rm ${Tmp}/*
    done
    
    
    rm -rf ${Tmp}
}

#-----------------------------------------------------------------------------
# Main
#-----------------------------------------------------------------------------
main(){
    #          1     2    3
    remakeData "150" ${1} "1"
}

for THIS_DIST in 0.06 0.07 0.08 0.09 0.10 0.11 0.12 0.13 0.14 0.15 0.16 0.17 0.18 0.19 0.20 0.21 ; do
    main ${THIS_DIST}
done
