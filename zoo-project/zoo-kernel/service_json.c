/*
 * Author : Gérald FENOY
 *
 *  Copyright 2017-2021 GeoLabs SARL. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "service_json.h"
#include "json.h"
#include <errno.h>
#include "json_tokener.h"
#include "json_object.h"
#include "json.h"
#include "stdlib.h"
#include "mimetypes.h"
#include "server_internal.h"
#include "service_internal.h"
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Convert a map to a json object
   * @param myMap the map to be converted into json object
   * @return a json_object pointer to the created json_object
   */
  json_object* mapToJson(map* myMap){
    json_object *res=json_object_new_object();
    map* cursor=myMap;
    map* sizeMap=getMap(myMap,"size");
    map* length=getMap(myMap,"length");
    while(cursor!=NULL){
      json_object *val=NULL;
      if(length==NULL && sizeMap==NULL)
	val=json_object_new_string(cursor->value);
      else{
	if(length==NULL && sizeMap!=NULL){
	  if(strncasecmp(cursor->name,"value",5)==0)
	    val=json_object_new_string_len(cursor->value,atoi(sizeMap->value));
	  else
	    val=json_object_new_string(cursor->value);
	}
	else{
	  // In other case we should consider array with potential sizes
	  int limit=atoi(length->value);
	  int i=0;
	  val=json_object_new_array();
	  for(i=0;i<limit;i++){
	    map* lsizeMap=getMapArray(myMap,"size",i);
	    if(lsizeMap!=NULL && strncasecmp(cursor->name,"value",5)==0){
	      json_object_array_add(val,json_object_new_string_len(cursor->value,atoi(sizeMap->value)));
	    }else{
	      json_object_array_add(val,json_object_new_string(cursor->value));
	    }
	  }
	}
      }
      if(val!=NULL)
	json_object_object_add(res,cursor->name,val);
      cursor=cursor->next;
    }
    return res;
  }

  /**
   * Convert a maps to a json object
   * @param myMap the maps to be converted into json object
   * @return a json_object pointer to the created json_object
   */
  json_object* mapsToJson(maps* myMap){
    json_object *res=json_object_new_object();
    maps* cursor=myMap;
    while(cursor!=NULL){
      json_object *obj=NULL;
      if(cursor->content!=NULL){
	obj=mapToJson(cursor->content);
      }else
	obj=json_object_new_object();
      if(cursor->child!=NULL){
	json_object *child=NULL;
	child=mapsToJson(cursor->child);
	json_object_object_add(obj,"child",child);
      }
      json_object_object_add(res,cursor->name,obj);
      cursor=cursor->next;
    }
    return res;
  }

  /**
   * Convert an elements to a json object
   * @param myElements the elements pointer to be converted into a json object
   * @return a json_object pointer to the created json_object
   */
  json_object* elementsToJson(elements* myElements){
    json_object *res=json_object_new_object();
    elements* cur=myElements;
    while(cur!=NULL){
      json_object *cres=json_object_new_object();
      json_object_object_add(cres,"content",mapToJson(cur->content));
      json_object_object_add(cres,"metadata",mapToJson(cur->metadata));
      json_object_object_add(cres,"additional_parameters",mapToJson(cur->additional_parameters));
      if(cur->format!=NULL){
	json_object_object_add(cres,"format",json_object_new_string(cur->format));
      }
      if(cur->child==NULL){
	if(cur->defaults!=NULL)
	  json_object_object_add(cres,"defaults",mapToJson(cur->defaults->content));
	else
	  json_object_object_add(cres,"defaults",mapToJson(NULL));
	iotype* scur=cur->supported;
	json_object *resi=json_object_new_array();
	while(scur!=NULL){
	  json_object_array_add(resi,mapToJson(scur->content));
	  scur=scur->next;
	}
	json_object_object_add(cres,"supported",resi);
      }
      
      json_object_object_add(cres,"child",elementsToJson(cur->child));

      json_object_object_add(res,cur->name,cres);
      cur=cur->next;
    }
    return res;
  }
  
  /**
   * Convert an service to a json object
   *
   * @param myService the service pointer to be converted into a json object
   * @return a json_object pointer to the created json_object
   */
  json_object* serviceToJson(service* myService){
    json_object *res=json_object_new_object();
    json_object_object_add(res,"name",json_object_new_string(myService->name));
    json_object_object_add(res,"content",mapToJson(myService->content));
    json_object_object_add(res,"metadata",mapToJson(myService->metadata));
    json_object_object_add(res,"additional_parameters",mapToJson(myService->additional_parameters));
    json_object_object_add(res,"inputs",elementsToJson(myService->inputs));
    json_object_object_add(res,"outputs",elementsToJson(myService->outputs));
    return res;
  }

  /**
   * Add Allowed Range properties to a json_object
   *
   * @param m the main configuration maps pointer
   * @param iot the iotype pointer
   * @param prop the json_object pointer to add the allowed range properties
   * @return a json_object pointer to the created json_object
   */
  void printAllowedRangesJ(maps* m,iotype* iot,json_object* prop){
    // TODO: avoid adding both maximum and exclusiveMaximum (same for minimum)
    // TODO: parse type/pass it as parameter and use it for proper type use 
    map* tmpMap1;
    json_object* prop4=json_object_new_object();
    for(int i=0;i<4;i++){
      tmpMap1=getMap(iot->content,rangeCorrespondances[i][0]);
      if(tmpMap1!=NULL){
	if(i<3)
	  json_object_object_add(prop,rangeCorrespondances[i][1],json_object_new_string(tmpMap1->value));
	else{
	  char* currentValue=NULL;	  
	  map* pmTmp=getMap(iot->content,rangeCorrespondances[0][0]);
	  if(tmpMap1->value[0]=='o' && pmTmp!=NULL){
	    json_object_object_add(prop,"exclusiveMinimum",json_object_new_string(pmTmp->value));
	    if(strlen(tmpMap1->value)==1){
	      pmTmp=getMap(iot->content,rangeCorrespondances[1][0]);
	      if(pmTmp!=NULL)
		json_object_object_add(prop,"exclusiveMaximum",json_object_new_string(pmTmp->value));
	    }
	  }
	  if(strlen(tmpMap1->value)>1 && tmpMap1->value[1]=='o'){
	    pmTmp=getMap(iot->content,rangeCorrespondances[1][0]);
	    if(pmTmp!=NULL)
	      json_object_object_add(prop,"exclusiveMaximum",json_object_new_string(pmTmp->value));
	  }
	}
      }
    }
  }
  
  /**
   * Add literalDataDomains property to a json_object
   *
   * @param m the main configuration maps pointer
   * @param in the elements pointer
   * @param input the json_object pointer to add the literalDataDomains property
   * @return a json_object pointer to the created json_object
   */
  void printLiteralDataJ(maps* m,elements* in,json_object* input){
    json_object* schema=json_object_new_object();
    map* pmMin=getMap(in->content,"minOccurs");
    if(in->defaults!=NULL){
      map* tmpMap1=getMap(in->defaults->content,"DataType");
      map* tmpMap2=getMap(in->defaults->content,"DataType");
      if(tmpMap1!=NULL){
	if(strncasecmp(tmpMap1->value,"float",5)==0)
	  json_object_object_add(schema,"type",json_object_new_string("number"));
	else
	  json_object_object_add(schema,"type",json_object_new_string(tmpMap1->value));
      }
      tmpMap1=getMap(in->defaults->content,"value");
      char *pcType="integer";
      if(tmpMap1!=NULL)
	// TODO check types!
	if(tmpMap2!=NULL && strncasecmp(tmpMap2->value,"integer",7)==0)
	  json_object_object_add(schema,"default",json_object_new_int(atoi(tmpMap1->value)));
	else{
	  if(tmpMap2!=NULL && strncasecmp(tmpMap2->value,"float",5)==0){
	    pcType="float";
	    json_object_object_add(schema,"default",json_object_new_double(atof(tmpMap1->value)));
	  }
	  else{
	    if(tmpMap2!=NULL && strncasecmp(tmpMap2->value,"bool",4)==0){
	      pcType="boolean";
	      if(strncasecmp(tmpMap1->value,"true",4)==0)
		json_object_object_add(schema,"default",json_object_new_boolean(true));
	      else
		json_object_object_add(schema,"default",json_object_new_boolean(false));
	    }
	    else
	      json_object_object_add(schema,"default",json_object_new_string(tmpMap1->value));
	  }
	}
      json_object* prop3=json_object_new_object();
      tmpMap1=getMap(in->defaults->content,"rangeMin");
      if(tmpMap1!=NULL){
	json_object* prop5=json_object_new_array();
	printAllowedRangesJ(m,in->defaults,schema);
	if(in->supported!=NULL){
	  iotype* iot=in->supported;
	  while(iot!=NULL){
	    printAllowedRangesJ(m,iot,schema);
	    iot=iot->next;
	  }
	}
      }
      else{
	tmpMap1=getMap(in->defaults->content,"range");
	if(tmpMap1!=NULL){
	  // TODO: parse range = [rangeMin,rangeMax] (require generic function)
	}else{ 
	  // AllowedValues
	  tmpMap1=getMap(in->defaults->content,"AllowedValues");
	  if(tmpMap1!=NULL){
	    char* saveptr;
	    json_object* prop5=json_object_new_array();
	    char *tmps = strtok_r (tmpMap1->value, ",", &saveptr);
	    while(tmps!=NULL){
	      json_object_array_add(prop5,json_object_new_string(tmps));
	      tmps = strtok_r (NULL, ",", &saveptr);
	    }
	    json_object_object_add(schema,"enum",prop5);
	  }else{
	    tmpMap1=getMap(in->defaults->content,"DataType");
	    if(tmpMap1!=NULL && strncasecmp(tmpMap1->value,"string",6)==0)
	      json_object_object_add(schema,"default",json_object_new_string("Any value"));
	  }
	}
      }
    }
    if(pmMin!=NULL && atoi(pmMin->value)==0)
	json_object_object_add(schema,"nullable",json_object_new_boolean(true));
    json_object_object_add(input,"schema",schema);
  }

  /**
   * Add support in the extended-schema for links object
   * 
   * @param m the main configuration maps pointer
   * @param el the elements pointer
   * @param res the json_object pointer on the array
   * @param isDefault boolean, try if it is the default value
   * @param maxSize map pointer (not used)
   */
  void printComplexHref(maps* m,elements* el,json_object* res,bool isDefault,map* maxSize){
    if(el!=NULL && el->defaults!=NULL){
      json_object* prop=json_object_new_object();
      json_object* prop1=json_object_new_array();
      json_object* prop2=json_object_new_object();
      map* tmpMap0=getMapFromMaps(m,"openapi","link_href");
      if(tmpMap0!=NULL)
	json_object_object_add(prop2,"$ref",json_object_new_string(tmpMap0->value));
      json_object_array_add(prop1,prop2);
      json_object* prop3=json_object_new_object();
      json_object_object_add(prop3,"type",json_object_new_string("object"));
      json_object* prop4=json_object_new_object();
      json_object* prop5=json_object_new_array();
      map* tmpMap2=getMap(el->defaults->content,"mimeType");
      if(tmpMap2!=NULL)
	json_object_array_add(prop5,json_object_new_string(tmpMap2->value));
      iotype* sup=el->supported;
      while(sup!=NULL){
	tmpMap2=getMap(sup->content,"mimeType");
	if(tmpMap2!=NULL)
	  json_object_array_add(prop5,json_object_new_string(tmpMap2->value));
	sup=sup->next;
      }
      json_object_object_add(prop4,"enum",prop5);
      json_object* prop6=json_object_new_object();
      json_object_object_add(prop6,"type",prop4);
      json_object_object_add(prop3,"properties",prop6);
      json_object_array_add(prop1,prop3);
      json_object_object_add(prop,"allOf",prop1);
      json_object_array_add(res,prop);
    }
  }

  /**
   * Add support for type array in extended-schema
   *
   *
   * @param m the main configuration maps pointer
   * @param el the elements pointer
   */
  json_object* addArray(maps* m,elements* el){
    json_object* res=json_object_new_object();
    json_object_object_add(res,"type",json_object_new_string("array"));
    map* tmpMap=getMap(el->content,"minOccurs");
    if(tmpMap!=NULL /*&& strcmp(tmpMap->value,"1")!=0*/){
      json_object_object_add(res,"minItems",json_object_new_int(atoi(tmpMap->value)));
    }
    tmpMap=getMap(el->content,"maxOccurs");
    if(tmpMap!=NULL){
      if(atoi(tmpMap->value)>1){
	json_object_object_add(res,"maxItems",json_object_new_int(atoi(tmpMap->value)));
	return res;
      }
      else{
	json_object_put(res);
	res=NULL;
	return res;
      }
    }
  }

  /**
   * Add basic schema definition for the BoundingBox type
   * 
   * @param m the main configuration maps pointer
   * @param in the main configuration maps pointer
   * @param input the json_object pointer used to store the schema definition
   */
  void printBoundingBoxJ(maps* m,elements* in,json_object* input){
    map* pmMin=getMap(in->content,"minOccurs");
    json_object* prop20=json_object_new_object();
    json_object_object_add(prop20,"type",json_object_new_string("object"));
    json_object* prop21=json_object_new_array();
    json_object_array_add(prop21,json_object_new_string("bbox"));
    json_object_array_add(prop21,json_object_new_string("crs"));
    json_object_object_add(prop20,"required",prop21);

    json_object* prop22=json_object_new_object();

    json_object* prop23=json_object_new_object();
    json_object_object_add(prop23,"type",json_object_new_string("array"));
    json_object* prop27=json_object_new_object();
    json_object* prop28=json_object_new_object();
    json_object* prop29=json_object_new_array();
    json_object_object_add(prop27,"minItems",json_object_new_int(4));
    json_object_object_add(prop27,"maxItems",json_object_new_int(4));
    json_object_array_add(prop29,prop27);
    json_object_object_add(prop28,"minItems",json_object_new_int(6));
    json_object_object_add(prop28,"maxItems",json_object_new_int(6));
    json_object_array_add(prop29,prop28);
    json_object_object_add(prop23,"oneOf",prop29);
    json_object* prop24=json_object_new_object();
    json_object_object_add(prop24,"type",json_object_new_string("number"));
    json_object_object_add(prop24,"format",json_object_new_string("double"));
    json_object_object_add(prop23,"items",prop24);
    json_object_object_add(prop22,"bbox",prop23);

    json_object* prop25=json_object_new_object();
    json_object_object_add(prop25,"type",json_object_new_string("string"));
    json_object_object_add(prop25,"format",json_object_new_string("uri"));

    json_object* prop26=json_object_new_array();

    map* tmpMap1=getMap(in->defaults->content,"crs");
    if(tmpMap1==NULL)
      return;
    json_object_array_add(prop26,json_object_new_string(tmpMap1->value));
    json_object_object_add(prop25,"default",json_object_new_string(tmpMap1->value));
    iotype* sup=in->supported;
    while(sup!=NULL){
      tmpMap1=getMap(sup->content,"crs");
      if(tmpMap1!=NULL)
	json_object_array_add(prop26,json_object_new_string(tmpMap1->value));
      sup=sup->next;
    }
    json_object_object_add(prop25,"enum",prop26);

    json_object_object_add(prop22,"crs",prop25);

    json_object_object_add(prop20,"properties",prop22);
    if(pmMin!=NULL && atoi(pmMin->value)==0)
      json_object_object_add(prop20,"nullable",json_object_new_boolean(true));
    json_object_object_add(input,"schema",prop20);
  }

  /**
   * Add schema property to a json_object (ComplexData)
   * @param m the main configuration maps pointer
   * @param iot the current iotype pointer
   * @param res the json_object pointer to add the properties to
   * @param isDefault boolean specifying if the currrent iotype is default
   * @param maxSize a map pointer to the maximumMegabytes param defined in the zcfg file for this input/output
   */
  void printFormatJ1(maps* m,iotype* iot,json_object* res,bool isDefault,map* maxSize){
    if(iot!=NULL){
      int i=0;
      json_object* prop1=json_object_new_object();
      map* tmpMap1=getMap(iot->content,"encoding");
      map* tmpMap2=getMap(iot->content,"mimeType");
      map* tmpMap3=getMap(iot->content,"schema");
      if(tmpMap2!=NULL && (strncasecmp(tmpMap2->value,"application/json",16)==0 || strstr(tmpMap2->value,"json")!=NULL)){
	json_object_object_add(prop1,"type",json_object_new_string("object"));
	if(tmpMap3!=NULL)
	  json_object_object_add(prop1,"$ref",json_object_new_string(tmpMap3->value));
	json_object_array_add(res,prop1);
	return ;
      }
      //json_object* prop2=json_object_new_object();
      json_object_object_add(prop1,"type",json_object_new_string("string"));
      if(tmpMap1!=NULL)
	json_object_object_add(prop1,"contentEncoding",json_object_new_string(tmpMap1->value));
      else{
	json_object_object_add(prop1,"contentEncoding",json_object_new_string("base64"));
	i=1;
      }
      if(tmpMap2!=NULL){
	json_object_object_add(prop1,"contentMediaType",json_object_new_string(tmpMap2->value));
      }
      // TODO: specific handling of schema?!
      if(tmpMap3!=NULL)
	json_object_object_add(prop1,"contentSchema",json_object_new_string(tmpMap3->value));
      if(maxSize!=NULL)
	json_object_object_add(prop1,"contentMaximumMegabytes",json_object_new_int64(atoll(maxSize->value)));
      json_object_array_add(res,prop1);
    }
  }
  
  /**
   * Add format properties to a json_object
   * @param m the main configuration maps pointer
   * @param iot the current iotype pointer
   * @param res the json_object pointer to add the properties to
   * @param isDefault boolean specifying if the currrent iotype is default
   * @param maxSize a map pointer to the maximumMegabytes param defined in the zcfg file for this input/output
   */
  void printFormatJ(maps* m,iotype* iot,json_object* res,bool isDefault,map* maxSize){
    if(iot!=NULL){
      map* tmpMap1=getMap(iot->content,"mimeType");
      json_object* prop1=json_object_new_object();
      json_object_object_add(prop1,"default",json_object_new_boolean(isDefault));
      json_object_object_add(prop1,"mediaType",json_object_new_string(tmpMap1->value));
      tmpMap1=getMap(iot->content,"encoding");
      if(tmpMap1!=NULL)
	json_object_object_add(prop1,"characterEncoding",json_object_new_string(tmpMap1->value));
      tmpMap1=getMap(iot->content,"schema");
      if(tmpMap1!=NULL)
	json_object_object_add(prop1,"schema",json_object_new_string(tmpMap1->value));
      if(maxSize!=NULL)
	json_object_object_add(prop1,"maximumMegabytes",json_object_new_int64(atoll(maxSize->value)));
      json_object_array_add(res,prop1);
    }
  }

  /**
   * Add additionalParameters property to a json_object
   * @param conf the main configuration maps pointer
   * @param meta a map pointer to the current metadata informations
   * @param doc the json_object pointer to add the property to
   */
  void printJAdditionalParameters(maps* conf,map* meta,json_object* doc){
    map* cmeta=meta;
    json_object* carr=json_object_new_array();
    int hasElement=-1;
    json_object* jcaps=json_object_new_object();
    while(cmeta!=NULL){
      json_object* jcmeta=json_object_new_object();
      if(strcasecmp(cmeta->name,"role")!=0 &&
	 strcasecmp(cmeta->name,"href")!=0 &&
	 strcasecmp(cmeta->name,"title")!=0 &&
	 strcasecmp(cmeta->name,"length")!=0 ){
	json_object_object_add(jcmeta,"name",json_object_new_string(cmeta->name));
	json_object_object_add(jcmeta,"value",json_object_new_string(cmeta->value));
	json_object_array_add(carr,jcmeta);
	hasElement++;
      }else{
	if(strcasecmp(cmeta->name,"length")!=0)
	  json_object_object_add(jcaps,cmeta->name,json_object_new_string(cmeta->value));
      }
      cmeta=cmeta->next;
    }
    if(hasElement>=0){
      json_object_object_add(jcaps,"additionalParameter",carr);
      json_object_object_add(doc,"additionalParameters",jcaps);
    }
  }

  /**
   * Add metadata property to a json_object
   * @param conf the main configuration maps pointer
   * @param meta a map pointer to the current metadata informations
   * @param doc the json_object pointer to add the property to
   */
  void printJMetadata(maps* conf,map* meta,json_object* doc){
    map* cmeta=meta;
    json_object* carr=json_object_new_array();
    int hasElement=-1;
    while(cmeta!=NULL){
      json_object* jcmeta=json_object_new_object();
      if(strcasecmp(cmeta->name,"role")==0 ||
	 strcasecmp(cmeta->name,"href")==0 ||
	 strcasecmp(cmeta->name,"title")==0 ){
	json_object_object_add(jcmeta,cmeta->name,json_object_new_string(cmeta->value));
	hasElement++;
      }
      json_object_array_add(carr,jcmeta);
      cmeta=cmeta->next;
    }
    if(hasElement>=0)
      json_object_object_add(doc,"metadata",carr);
  }

  /**
   * Add metadata properties to a json_object
   * @param m the main configuration maps pointer
   * @param io a string 
   * @param in an elements pointer to the current input/output
   * @param inputs the json_object pointer to add the property to
   * @param serv the service pointer to extract the metadata from
   */
  void printIOTypeJ(maps* m, const char *io, elements* in,json_object* inputs,service* serv){
    while(in!=NULL){
      json_object* input=json_object_new_object();
      map* tmpMap=getMap(in->content,"title");
      map* pmMin=NULL;
      map* pmMax=NULL;
      if(strcmp(io,"input")==0){
	pmMin=getMap(in->content,"minOccurs");
	pmMax=getMap(in->content,"maxOccurs");
      }
      if(tmpMap!=NULL)
	json_object_object_add(input,"title",json_object_new_string(tmpMap->value));
      tmpMap=getMap(in->content,"abstract");
      if(tmpMap!=NULL)
	json_object_object_add(input,"description",json_object_new_string(tmpMap->value));
      if(strcmp(io,"input")==0){
	if(pmMin!=NULL && strcmp(pmMin->value,"1")!=0 && strcmp(pmMin->value,"0")!=0)
	  json_object_object_add(input,"minOccurs",json_object_new_int(atoi(pmMin->value)));
	if(pmMax!=NULL){
	  if(strncasecmp(pmMax->value,"unbounded",9)==0)
	    json_object_object_add(input,"maxOccurs",json_object_new_string(pmMax->value));
	  else
	    if(strcmp(pmMax->value,"1")!=0)
	      json_object_object_add(input,"maxOccurs",json_object_new_int(atoi(pmMax->value)));
	}
      }
      if(in->format!=NULL){
	json_object* input1=json_object_new_object();
	json_object* prop0=json_object_new_array();
	json_object* prop1=json_object_new_array();
	json_object* pjoSchema=json_object_new_array();
	if(strcasecmp(in->format,"LiteralData")==0 ||
	   strcasecmp(in->format,"LiteralOutput")==0){
	  printLiteralDataJ(m,in,input);
	}else{
	  if(strcasecmp(in->format,"ComplexData")==0 ||
	     strcasecmp(in->format,"ComplexOutput")==0) {
	    map* sizeMap=getMap(in->content,"maximumMegabytes");
	    printComplexHref(m,in,prop1,false,sizeMap);
	    printFormatJ(m,in->defaults,prop0,true,sizeMap);
	    json_object* pjoSchemaPart=json_object_new_array();
	    printFormatJ1(m,in->defaults,pjoSchemaPart,true,sizeMap);


	    printFormatJ1(m,in->defaults,pjoSchema,true,sizeMap);
	    iotype* sup=in->supported;
	    while(sup!=NULL){
	      printFormatJ(m,sup,prop0,false,sizeMap);
	      printFormatJ1(m,sup,pjoSchemaPart,false,sizeMap);
	      printFormatJ1(m,sup,pjoSchema,true,sizeMap);
	      sup=sup->next;
	    }

	    json_object* pjoQualifiedValue=json_object_new_object();
	    json_object_object_add(pjoQualifiedValue,"type",json_object_new_string("object"));

	    json_object* pjoRequiredArray=json_object_new_array();
	    json_object_array_add(pjoRequiredArray,json_object_new_string("value"));
	    json_object_object_add(pjoQualifiedValue,"required",pjoRequiredArray);

	    json_object* pjoProperties=json_object_new_object();
	    json_object* pjoValueField=json_object_new_object();
	    json_object_object_add(pjoValueField,"oneOf",pjoSchemaPart);
	    json_object_object_add(pjoProperties,"value",pjoValueField);
	    json_object_object_add(pjoQualifiedValue,"properties",pjoProperties);
	    json_object_array_add(prop1,pjoQualifiedValue);

	    json_object* prop2=json_object_new_object();
	    json_object_object_add(prop2,"oneOf",prop1);
	    json_object* prop3=addArray(m,in);
	    if(prop3!=NULL){
	      json_object_object_add(prop3,"items",prop2);
	      if(pmMin!=NULL && atoi(pmMin->value)==0)
		json_object_object_add(prop3,"nullable",json_object_new_boolean(true));
	      json_object_object_add(input,"extended-schema",prop3);
	    }
	    else{
	      if(pmMin!=NULL && atoi(pmMin->value)==0)
		json_object_object_add(prop2,"nullable",json_object_new_boolean(true));
	      json_object_object_add(input,"extended-schema",prop2);
	    }
	    json_object* prop4=json_object_new_object();
	    json_object_object_add(prop4,"oneOf",pjoSchema);
	    json_object_object_add(input,"schema",prop4);
	  }
	  else{
	    printBoundingBoxJ(m,in,input);
	  }
	}
      }
      printJMetadata(m,in->metadata,input);
      printJAdditionalParameters(m,in->additional_parameters,input);
      json_object_object_add(inputs,in->name,input);
      in=in->next;
    }
    
  }

  /**
   * Add all the capabilities properties to a json_object
   * @param ref the registry pointer
   * @param m the main configuration maps pointer
   * @param doc0 the void (json_object) pointer to add the property to
   * @param nc0 the void (json_object) pointer to add the property to
   * @param serv the service pointer to extract the metadata from
   */
  void printGetCapabilitiesForProcessJ(registry *reg, maps* m,void* doc0,void* nc0,service* serv){
    json_object* doc=(json_object*) doc0;
    json_object* nc=(json_object*) nc0;
    json_object *res;
    if(doc!=NULL)
      res=json_object_new_object();
    else
      res=(json_object*) nc0;
      
    map* tmpMap0=getMapFromMaps(m,"lenv","level");
    char* rUrl=serv->name;
    if(tmpMap0!=NULL && atoi(tmpMap0->value)>0){
      int i=0;
      maps* tmpMaps=getMaps(m,"lenv");
      char* tmpName=NULL;
      for(i=0;i<atoi(tmpMap0->value);i++){
	char* key=(char*)malloc(15*sizeof(char));
	sprintf(key,"sprefix_%d",i);
	map* tmpMap1=getMap(tmpMaps->content,key);
	free(key);
	if(tmpMap1!=NULL){
	  if(i+1==atoi(tmpMap0->value))
	    if(tmpName==NULL){
	      tmpName=(char*) malloc((strlen(serv->name)+strlen(tmpMap1->value)+1)*sizeof(char));
	      sprintf(tmpName,"%s%s",tmpMap1->value,serv->name);
	    }else{
	      char* tmpStr=zStrdup(tmpName);
	      tmpName=(char*) realloc(tmpName,(strlen(tmpStr)+strlen(tmpMap1->value)+strlen(serv->name)+1)*sizeof(char));
	      sprintf(tmpName,"%s%s%s",tmpStr,tmpMap1->value,serv->name);
	      free(tmpStr);
	    }
	  else
	    if(tmpName==NULL){
	      tmpName=(char*) malloc((strlen(tmpMap1->value)+1)*sizeof(char));
	      sprintf(tmpName,"%s",tmpMap1->value);
	    }else{
	      char* tmpStr=zStrdup(tmpName);
	      tmpName=(char*) realloc(tmpName,(strlen(tmpStr)+strlen(tmpMap1->value)+1)*sizeof(char));
	      sprintf(tmpName,"%s%s",tmpStr,tmpMap1->value);
	      free(tmpStr);
	  }
	}
      }
      if(tmpName!=NULL){
	json_object_object_add(res,"id",json_object_new_string(tmpName));
	rUrl=zStrdup(tmpName);
	free(tmpName);
      }
    }
    else{
      json_object_object_add(res,"id",json_object_new_string(serv->name));
      rUrl=serv->name;
    }
    if(serv->content!=NULL){
      map* tmpMap=getMap(serv->content,"title");
      if(tmpMap!=NULL){
	json_object_object_add(res,"title",json_object_new_string(tmpMap->value));
      }
      tmpMap=getMap(serv->content,"abstract");
      if(tmpMap!=NULL){
	json_object_object_add(res,"description",json_object_new_string(tmpMap->value));
      }
      tmpMap=getMap(serv->content,"processVersion");
      if(tmpMap!=NULL){
	if(strlen(tmpMap->value)<5){
	  char *val=(char*)malloc((strlen(tmpMap->value)+5)*sizeof(char));
	  sprintf(val,"%s.0.0",tmpMap->value);
	  json_object_object_add(res,"version",json_object_new_string(val));
	  free(val);
	}
	else
	  json_object_object_add(res,"version",json_object_new_string(tmpMap->value));
      }else
	json_object_object_add(res,"version",json_object_new_string("1.0.0"));
      int limit=4;
      int i=0;
      map* sType=getMap(serv->content,"serviceType");
      for(;i<limit;i+=2){
	json_object *res2=json_object_new_array();
	char *saveptr;
	char* dupStr=strdup(jcapabilities[i+1]);
	char *tmps = strtok_r (dupStr, " ", &saveptr);
	while(tmps!=NULL){
	  json_object_array_add(res2,json_object_new_string(tmps));
	  tmps = strtok_r (NULL, " ", &saveptr);
	}
	free(dupStr);
	json_object_object_add(res,jcapabilities[i],res2);
      }
      json_object *res1=json_object_new_array();
      json_object *res2=json_object_new_object();
      json_object *res3=json_object_new_object();
      map* pmTmp=getMapFromMaps(m,"lenv","requestType");
      if(pmTmp!=NULL && strncasecmp(pmTmp->value,"desc",4)==0){
	json_object_object_add(res2,"rel",json_object_new_string("process-desc"));
      }
      else{
	json_object_object_add(res2,"rel",json_object_new_string("execute"));
      }
      json_object_object_add(res3,"rel",json_object_new_string("alternate"));
      json_object_object_add(res3,"type",json_object_new_string("text/html"));
      json_object_object_add(res2,"type",json_object_new_string("application/json"));
      map* tmpUrl=getMapFromMaps(m,"openapi","rootUrl");
      char* tmpStr=(char*) malloc((strlen(tmpUrl->value)+strlen(rUrl)+13)*sizeof(char));
      sprintf(tmpStr,"%s/processes/%s",tmpUrl->value,rUrl);
      if(pmTmp!=NULL && strncasecmp(pmTmp->value,"desc",4)!=0){
	json_object_object_add(res2,"title",json_object_new_string("Execute End Point"));
	json_object_object_add(res3,"title",json_object_new_string("Execute End Point"));
	char* tmpStr1=zStrdup(tmpStr);
	tmpStr=(char*) realloc(tmpStr,(strlen(tmpStr1)+11)*sizeof(char));
	sprintf(tmpStr,"%s/execution",tmpStr1);
	free(tmpStr1);
      }else{
	json_object_object_add(res2,"title",json_object_new_string("Process Description"));
	json_object_object_add(res3,"title",json_object_new_string("Process Description"));
      }
      char* tmpStr3=(char*) malloc((strlen(tmpStr)+6)*sizeof(char));
      sprintf(tmpStr3,"%s.html",tmpStr);
      json_object_object_add(res3,"href",json_object_new_string(tmpStr3));
      free(tmpStr3);
      json_object_object_add(res2,"href",json_object_new_string(tmpStr));
      free(tmpStr);
      json_object_array_add(res1,res2);
      tmpUrl=getMapFromMaps(m,"openapi","partial_html_support");
      if(tmpUrl!=NULL && strncasecmp(tmpUrl->value,"true",4)==0)
	json_object_array_add(res1,res3);
      else
	json_object_put(res3);
      json_object_object_add(res,"links",res1);
    }
    if(doc==NULL){
      elements* in=serv->inputs;
      json_object* inputs=json_object_new_object();
      printIOTypeJ(m,"input",in,inputs,serv);
      json_object_object_add(res,"inputs",inputs);

      in=serv->outputs;
      json_object* outputs=json_object_new_object();
      printIOTypeJ(m,"output",in,outputs,serv);
      json_object_object_add(res,"outputs",outputs);

    }
    if(strcmp(rUrl,serv->name)!=0)
      free(rUrl);
    if(doc!=NULL)
      json_object_array_add(doc,res);
  }

  /**
   * Print an OWS ExceptionReport Document and HTTP headers (when required) 
   * depending on the code.
   * Set hasPrinted value to true in the [lenv] section.
   * 
   * @param m the maps containing the settings of the main.cfg file
   * @param s the map containing the text,code,locator keys (or a map array of the same keys)
   */
  void printExceptionReportResponseJ(maps* m,map* s){
    if(getMapFromMaps(m,"lenv","hasPrinted")!=NULL)
      return;
    int buffersize;
    json_object *res=json_object_new_object();

    maps* tmpMap=getMaps(m,"main");
    const char *exceptionCode;
    
    map* tmp=getMap(s,"code");
    exceptionCode=produceStatusString(m,tmp);    
    if(tmp!=NULL){
      json_object_object_add(res,"title",json_object_new_string(tmp->value));
      int i=0;
      for(i=0;i<3;i++){
	if(strcasecmp(tmp->value,WPSExceptionCode[OAPIPCorrespondances[i][0]])==0){
	  map* pmExceptionUrl=getMapFromMaps(m,"openapi","exceptionsUrl");
	  char* pcaTmp=(char*)malloc((strlen(pmExceptionUrl->value)+strlen(OAPIPExceptionCode[OAPIPCorrespondances[i][1]])+2)*sizeof(char));
	  sprintf(pcaTmp,"%s/%s",pmExceptionUrl->value,OAPIPExceptionCode[OAPIPCorrespondances[i][1]]);
	  json_object_object_add(res,"type",json_object_new_string(pcaTmp));
	  free(pcaTmp);
	}
      }
    }
    else{
      json_object_object_add(res,"title",json_object_new_string("NoApplicableCode"));
    }
    if(getMapFromMaps(m,"lenv","no-headers")==NULL)
      printHeaders(m);

    tmp=getMapFromMaps(m,"lenv","status_code");
    if(tmp!=NULL)
      exceptionCode=tmp->value;
    if(getMapFromMaps(m,"lenv","no-headers")==NULL){
      if(m!=NULL){
	map *tmpSid=getMapFromMaps(m,"lenv","sid");
	if(tmpSid!=NULL){
	  if( getpid()==atoi(tmpSid->value) ){
	    printf("Status: %s\r\n\r\n",exceptionCode);
	  }
	}
	else{
	  printf("Status: %s\r\n\r\n",exceptionCode);
	}
      }else{
	printf("Status: %s\r\n\r\n",exceptionCode);
      }
    }
    tmp=getMap(s,"text");
    if(tmp==NULL)
      tmp=getMap(s,"message");
    if(tmp==NULL)
      tmp=getMapFromMaps(m,"lenv","message");
    if(tmp!=NULL)
      json_object_object_add(res,"detail",json_object_new_string(tmp->value));

    const char* jsonStr=json_object_to_json_string_ext(res,JSON_C_TO_STRING_NOSLASHESCAPE);
    if(getMapFromMaps(m,"lenv","jsonStr")==NULL)
      setMapInMaps(m,"lenv","jsonStr",jsonStr);
    maps* pmsTmp=getMaps(m,"lenv");
    printf(jsonStr);
    if(m!=NULL)
      setMapInMaps(m,"lenv","hasPrinted","true");
    json_object_put(res);
  }

  /**
   * Parse LiteralData value
   * @param conf the maps containing the settings of the main.cfg file
   * @param req json_object pointing to the input/output
   * @param element
   * @param output the maps to set current json structure
   */
  void parseJLiteral(maps* conf,json_object* req,elements* element,maps* output){
    json_object* json_cinput=NULL;
    if(json_object_object_get_ex(req,"value",&json_cinput)!=FALSE){
      output->content=createMap("value",json_object_get_string(json_cinput));
    }
    const char* tmpStrs[2]={
      "dataType",
      "uom"
    };
    for(int i=0;i<2;i++)
      if(json_object_object_get_ex(req,tmpStrs[i],&json_cinput)!=FALSE){
	json_object* json_cinput1;
	if(json_object_object_get_ex(json_cinput,"name",&json_cinput1)!=FALSE){
	  if(output->content==NULL)
	    output->content=createMap(tmpStrs[i],json_object_get_string(json_cinput1));
	  else
	    addToMap(output->content,tmpStrs[i],json_object_get_string(json_cinput1));
	}
	if(json_object_object_get_ex(json_cinput,"reference",&json_cinput1)!=FALSE){
	  if(output->content==NULL)
	    output->content=createMap(tmpStrs[i],json_object_get_string(json_cinput1));
	  else
	    addToMap(output->content,tmpStrs[i],json_object_get_string(json_cinput1));
	}
      }
  }

  /**
   * Search field names used in the OGC API - Processes specification and 
   * replace them by old names from WPS.
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param req json_object to fetch attributes from
   * @param output the maps to store metadata informations as a maps
   */ 
  void checkCorrespondingJFields(maps* conf,json_object* req,maps* output){
    json_object* json_cinput=NULL;
    for(int i=0;i<9;i++){
      if(json_object_object_get_ex(req,pccFields[i],&json_cinput)!=FALSE){
	if(output->content==NULL)
	  output->content=createMap(pccFields[i],json_object_get_string(json_cinput));
	else
	  addToMap(output->content,pccFields[i],json_object_get_string(json_cinput));

	if(i==0 || i==3 || i==6){
	  if(output->content==NULL)
	    output->content=createMap(pccRFields[0],json_object_get_string(json_cinput));
	  else
	    addToMap(output->content,pccRFields[0],json_object_get_string(json_cinput));
	}
	if(i==1 || i==4 || i==7){
	  if(output->content==NULL)
	    output->content=createMap(pccRFields[1],json_object_get_string(json_cinput));
	  else
	    addToMap(output->content,pccRFields[1],json_object_get_string(json_cinput));
	}
	if(i==2 || i==5 || i==8){
	  if(output->content==NULL)
	    output->content=createMap(pccRFields[2],json_object_get_string(json_cinput));
	  else
	    addToMap(output->content,pccRFields[2],json_object_get_string(json_cinput));
	}
      }
    }
  }
  
  /**
   * Parse ComplexData value
   * @param conf the maps containing the settings of the main.cfg file
   * @param req json_object pointing to the input/output
   * @param element
   * @param output the maps to set current json structure
   * @param name the name of the request from http_requests
   */
  void parseJComplex(maps* conf,json_object* req,elements* element,maps* output,const char* name){
    json_object* json_cinput=NULL;
    if(json_object_object_get_ex(req,"value",&json_cinput)!=FALSE){
      output->content=createMap("value",json_object_get_string(json_cinput));
    }
    else{
      json_object* json_value=NULL;
      if(json_object_object_get_ex(req,"href",&json_value)!=FALSE){
	output->content=createMap("xlink:href",json_object_get_string(json_value));
	int len=0;
	int createdStr=0;
	char *tmpStr="url";
	char *tmpStr1="input";
	if(getMaps(conf,"http_requests")==NULL){
	  maps* tmpMaps=createMaps("http_requests");
	  tmpMaps->content=createMap("length","1");
	  addMapsToMaps(&conf,tmpMaps);
	  freeMaps(&tmpMaps);
	  free(tmpMaps);
	}else{
	  map* tmpMap=getMapFromMaps(conf,"http_requests","length");
	  int len=atoi(tmpMap->value);
	  createdStr=1;
	  tmpStr=(char*) malloc((12)*sizeof(char));
	  sprintf(tmpStr,"%d",len+1);
	  setMapInMaps(conf,"http_requests","length",tmpStr);
	  sprintf(tmpStr,"url_%d",len);
	  tmpStr1=(char*) malloc((14)*sizeof(char));
	  sprintf(tmpStr1,"input_%d",len);
	}
	setMapInMaps(conf,"http_requests",tmpStr,json_object_get_string(json_value));
	setMapInMaps(conf,"http_requests",tmpStr1,name);
	if(createdStr>0){
	  free(tmpStr);
	  free(tmpStr1);
	}
      }
    }
    if(json_object_object_get_ex(req,"format",&json_cinput)!=FALSE){
      json_object_object_foreach(json_cinput, key, val) {
	if(output->content==NULL)
	  output->content=createMap(key,json_object_get_string(val));
	else
	  addToMap(output->content,key,json_object_get_string(val));
	
      }
      checkCorrespondingJFields(conf,json_cinput,output); 
    }
    checkCorrespondingJFields(conf,req,output); 
  }

  /**
   * Parse BoundingBox value
   * @param conf the maps containing the settings of the main.cfg file
   * @param req json_object pointing to the input/output
   * @param element
   * @param output the maps to set current json structure
   */
  void parseJBoundingBox(maps* conf,json_object* req,elements* element,maps* output){
    json_object* json_cinput=NULL;
    if(json_object_object_get_ex(req,"bbox",&json_cinput)!=FALSE){
      output->content=createMap("value",json_object_get_string(json_cinput));
    }
    if(json_object_object_get_ex(req,"crs",&json_cinput)!=FALSE){
      if(output->content==NULL)
	output->content=createMap("crs",json_object_get_string(json_cinput));
      else
	addToMap(output->content,"crs","http://www.opengis.net/def/crs/OGC/1.3/CRS84");
    }
    char* tmpStrs[2]={
      "lowerCorner",
      "upperCorner"
    };
    for(int i=0;i<2;i++)
      if(json_object_object_get_ex(req,tmpStrs[i],&json_cinput)!=FALSE){
	if(output->content==NULL)
	  output->content=createMap(tmpStrs[i],json_object_get_string(json_cinput));
	else
	  addToMap(output->content,tmpStrs[i],json_object_get_string(json_cinput));
      }
  }

  /**
   * Parse a single input / output entity
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param ioElements the elements extracted from the zcfg
   * @param ioMaps the produced maps containing inputs (or outputs) 
   * @param ioType the char* set to inputs or outputs
   * @param key char* the input/output name
   * @param val json_object pointing to the input/output
   * @param cMaps the outputed maps containing input (or output) metedata 
   */
  void _parseJIOSingle1(maps* conf,elements* cio, maps** ioMaps,const char* ioType,const char* key,json_object* val,maps* cMaps){
    json_object* json_cinput;
    if(strncmp(ioType,"inputs",6)==0) {
      if(json_object_is_type(val,json_type_object)) {
	// ComplexData
	if( json_object_object_get_ex(val,"value",&json_cinput) != FALSE ||
	  json_object_object_get_ex(val,"href",&json_cinput) != FALSE ) {
	  parseJComplex(conf,val,cio,cMaps,key);
	} else if( json_object_object_get_ex(val,"bbox",&json_cinput) != FALSE ){
	  parseJBoundingBox(conf,val,cio,cMaps);
	}
      } else if(json_object_is_type(val,json_type_array)){
	// Need to run detection on each item within the array
      }else{
	// Basic types
	cMaps->content=createMap("value",json_object_get_string(val));
      }
    }
    else{
      if(json_object_object_get_ex(val,"dataType",&json_cinput)!=FALSE){
	parseJLiteral(conf,val,cio,cMaps);
      } else if(json_object_object_get_ex(val,"format",&json_cinput)!=FALSE){
	parseJComplex(conf,val,cio,cMaps,key);
      } else if(json_object_object_get_ex(val,"bbox",&json_cinput)!=FALSE){
	parseJBoundingBox(conf,val,cio,cMaps);
      }// else error!
      else{
	if(json_object_object_get_ex(val,"value",&json_cinput)!=FALSE){
	  map* error=createMap("code","BadRequest");
	  char tmpS[1024];
	  sprintf(tmpS,_("Missing input for %s"),cio->name);
	  addToMap(error,"message",tmpS);
	  setMapInMaps(conf,"lenv","status_code","400 Bad Request");
	  printExceptionReportResponseJ(conf,error);
	  return;
	}else{
	  if(json_object_get_type(val)==json_type_string){
	    parseJLiteral(conf,val,cio,cMaps);
	  }else if(json_object_get_type(val)==json_type_object){
	    json_object* json_ccinput=NULL;
	    if(json_object_object_get_ex(val,"bbox",&json_ccinput)!=FALSE){
	      parseJBoundingBox(conf,val,cio,cMaps);
	    }
	    else{
	      parseJComplex(conf,val,cio,cMaps,key);
	    }
	  }else{
	    if(strcmp(ioType,"input")==0){
	      map* error=createMap("code","BadRequest");
	      char tmpS1[1024];
	      sprintf(tmpS1,_("Issue with input %s"),cio->name);
	      addToMap(error,"message",tmpS1);
	      setMapInMaps(conf,"lenv","status_code","400 Bad Request");
	      printExceptionReportResponseJ(conf,error);
	      return;
	    }
	  }
	}
      }
    }
  }
  
  /**
   * Parse a single input / output entity
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param ioElements the elements extracted from the zcfg
   * @param ioMaps the produced maps containing inputs (or outputs) 
   * @param ioType the char* set to inputs or outputs
   * @param key char* the input/output name
   * @param val json_object pointing to the input/output
   * @param cMaps the outputed maps containing input (or output) metedata 
   */
  void _parseJIOSingle(maps* conf,elements* cio, maps** ioMaps,const char* ioType,const char* key,json_object* val,maps* cMaps){
    json_object* json_cinput;
    if(json_object_object_get_ex(val,"dataType",&json_cinput)!=FALSE){
      parseJLiteral(conf,val,cio,cMaps);
    } else if(json_object_object_get_ex(val,"format",&json_cinput)!=FALSE){
      parseJComplex(conf,val,cio,cMaps,key);
    } else if(json_object_object_get_ex(val,"bbox",&json_cinput)!=FALSE){
      parseJBoundingBox(conf,val,cio,cMaps);
    }// else error!
    else{
      if(json_object_object_get_ex(val,"value",&json_cinput)!=FALSE){
	map* error=createMap("code","BadRequest");
	char tmpS[1024];
	sprintf(tmpS,_("Missing input for %s"),cio->name);
	addToMap(error,"message",tmpS);
	setMapInMaps(conf,"lenv","status_code","400 Bad Request");
	printExceptionReportResponseJ(conf,error);
	return;
      }else{
	if(json_object_get_type(json_cinput)==json_type_string){
	  parseJLiteral(conf,val,cio,cMaps);
	}else if(json_object_get_type(json_cinput)==json_type_object){
	  json_object* json_ccinput=NULL;
	  if(json_object_object_get_ex(json_cinput,"bbox",&json_ccinput)!=FALSE){
	    parseJComplex(conf,val,cio,cMaps,key);
	  }
	  else{
	    parseJBoundingBox(conf,val,cio,cMaps);
	  }
	}else{
	  if(strcmp(ioType,"input")==0){
	    map* error=createMap("code","BadRequest");
	    char tmpS1[1024];
	    sprintf(tmpS1,_("Issue with input %s"),cio->name);
	    addToMap(error,"message",tmpS1);
	    setMapInMaps(conf,"lenv","status_code","400 Bad Request");
	    printExceptionReportResponseJ(conf,error);
	    return;
	  }
	}
      }
    }    
  }
  
  /**
   * Parse a single input / output entity that can be an array 
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param ioElements the elements extracted from the zcfg
   * @param ioMaps the produced maps containing inputs (or outputs) 
   * @param ioType the char* set to inputs or outputs
   * @param key char* the input/output name
   * @param val json_object pointing to the input/output
   */
  void parseJIOSingle(maps* conf,elements* ioElements, maps** ioMaps,const char* ioType,const char* key,json_object* val){
    maps *cMaps=NULL;
    cMaps=createMaps(key);
    elements* cio=getElements(ioElements,key);    
    if(json_object_is_type(val,json_type_array)){
      int i=0;
      size_t len=json_object_array_length(val);
      for(i=0;i<len;i++){
	json_object* json_current_io=json_object_array_get_idx(val,i);
	maps* pmsExtra=createMaps(key);
	_parseJIOSingle1(conf,cio,ioMaps,ioType,key,json_current_io,pmsExtra);
	map* pmCtx=pmsExtra->content;
	while(pmCtx!=NULL){
	  if(cMaps->content==NULL)
	    cMaps->content=createMap(pmCtx->name,pmCtx->value);
	  else
	    setMapArray(cMaps->content,pmCtx->name,i,pmCtx->value);
	  pmCtx=pmCtx->next;
	}
	freeMaps(&pmsExtra);
	free(pmsExtra);
      }
    }else{
      _parseJIOSingle1(conf,cio,ioMaps,ioType,key,val,cMaps);
    }

    
    if(strcmp(ioType,"outputs")==0){
      // Outputs
      json_object* json_cinput;
      if(json_object_object_get_ex(val,"transmissionMode",&json_cinput)!=FALSE){
	if(cMaps->content==NULL)
	  cMaps->content=createMap("transmissionMode",json_object_get_string(json_cinput));
	else
	  addToMap(cMaps->content,"transmissionMode",json_object_get_string(json_cinput));
      }
      if(json_object_object_get_ex(val,"format",&json_cinput)!=FALSE){
	json_object_object_foreach(json_cinput, key1, val1) {
	  if(cMaps->content==NULL)
	    cMaps->content=createMap(key1,json_object_get_string(val1));
	  else
	    addToMap(cMaps->content,key1,json_object_get_string(val1));
	}
      }
    }

    addToMap(cMaps->content,"inRequest","true");
    if (ioMaps == NULL)
      *ioMaps = dupMaps(&cMaps);
    else
      addMapsToMaps (ioMaps, cMaps);

    freeMaps(&cMaps);
    free(cMaps);    
  }
  
  /**
   * Parse all the inputs / outputs entities
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param req json_object pointing to the input/output
   * @param ioElements the elements extracted from the zcfg
   * @param ioMaps the produced maps containing inputs (or outputs) 
   * @param ioType the char* set to inputs or outputs
   */
  void parseJIO(maps* conf, json_object* req, elements* ioElements, maps** ioMaps,const char* ioType){
    json_object* json_io=NULL;
    if(json_object_object_get_ex(req,ioType,&json_io)!=FALSE){
      json_object* json_current_io=NULL;
      if(json_object_is_type(json_io,json_type_array)){
	size_t len=json_object_array_length(json_io);
	for(int i=0;i<len;i++){
	  maps *cMaps=NULL;
	  json_current_io=json_object_array_get_idx(json_io,i);
	  json_object* cname=NULL;
	  if(json_object_object_get_ex(json_current_io,"id",&cname)!=FALSE) {
	    json_object* json_input=NULL;
	    if(json_object_object_get_ex(json_current_io,"input",&json_input)!=FALSE) {	    
	      parseJIOSingle(conf,ioElements,ioMaps,ioType,json_object_get_string(cname),json_input);
	    }else
	      parseJIOSingle(conf,ioElements,ioMaps,ioType,json_object_get_string(cname),json_current_io);
	  }
	}
      }else{
	json_object_object_foreach(json_io, key, val) {
	  parseJIOSingle(conf,ioElements,ioMaps,ioType,key,val);
	}
      }
    }
  }

  /**
   * Parse Json Request
   * @param conf the maps containing the settings of the main.cfg file
   * @param s the current service metadata
   * @param req the JSON object of the request body
   * @param inputs the produced maps
   * @param outputs the produced maps
   */
  void parseJRequest(maps* conf, service* s,json_object* req, map* request_inputs, maps** inputs,maps** outputs){
    elements* io=s->inputs;
    json_object* json_io=NULL;
    int parsed=0;
    char* tmpS="input";
    maps* in=*inputs;
    parseJIO(conf,req,s->inputs,inputs,"inputs");
    parseJIO(conf,req,s->outputs,outputs,"outputs");

    if(parsed==0){
      json_io=NULL;
      if(json_object_object_get_ex(req,"mode",&json_io)!=FALSE){
	addToMap(request_inputs,"mode",json_object_get_string(json_io));
	setMapInMaps(conf,"request","mode",json_object_get_string(json_io));
      }
      json_io=NULL;
      map* preference=getMapFromMaps(conf,"renv","HTTP_PREFER");
      if(preference!=NULL && strstr(preference->value,"return=minimal")!=NULL){
	addToMap(request_inputs,"response","raw");
	setMapInMaps(conf,"request","response","raw");
      }
      else{
	if(preference!=NULL && strstr(preference->value,"return=representation")!=NULL){
	  addToMap(request_inputs,"response","document");
	  setMapInMaps(conf,"request","response","document");
	}
      }
      if(json_object_object_get_ex(req,"response",&json_io)!=FALSE){
	addToMap(request_inputs,"response",json_object_get_string(json_io));
	setMapInMaps(conf,"request","response",json_object_get_string(json_io));
      }
      json_io=NULL;
      if(json_object_object_get_ex(req,"subscriber",&json_io)!=FALSE){
	maps* subscribers=createMaps("subscriber");
	json_object* json_subscriber=NULL;
	if(json_object_object_get_ex(json_io,"successUri",&json_subscriber)!=FALSE){
	  subscribers->content=createMap("successUri",json_object_get_string(json_subscriber));
	}
	if(json_object_object_get_ex(json_io,"inProgressUri",&json_subscriber)!=FALSE){
	  if(subscribers->content==NULL)
	    subscribers->content=createMap("inProgressUri",json_object_get_string(json_subscriber));
	  else
	    addToMap(subscribers->content,"inProgressUri",json_object_get_string(json_subscriber));
	}
	if(json_object_object_get_ex(json_io,"failedUri",&json_subscriber)!=FALSE){
	  if(subscribers->content==NULL)
	    subscribers->content=createMap("failedUri",json_object_get_string(json_subscriber));
	  else
	    addToMap(subscribers->content,"failedUri",json_object_get_string(json_subscriber));
	}
	addMapsToMaps(&conf,subscribers);
	freeMaps(&subscribers);
	free(subscribers);
      }
    }
  }

  /**
   * Print the jobs status info
   * cf. 
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @return the JSON object pointer to the jobs list
   */
  json_object* printJobStatus(maps* pmsConf,char* pcJobId){
    json_object* pjoRes=NULL;
    runGetStatus(pmsConf,pcJobId,"GetStatus");
    map* pmError=getMapFromMaps(pmsConf,"lenv","error");
    if(pmError!=NULL && strncasecmp(pmError->value,"true",4)==0){
      printExceptionReportResponseJ(pmsConf,getMapFromMaps(pmsConf,"lenv","code"));
      return NULL;
    }else{
      map* pmStatus=getMapFromMaps(pmsConf,"lenv","status");
      setMapInMaps(pmsConf,"lenv","gs_location","false");
      setMapInMaps(pmsConf,"lenv","gs_usid",pcJobId);
      if(pmStatus!=NULL && strncasecmp(pmStatus->value,"Failed",6)==0)
	pjoRes=createStatus(pmsConf,SERVICE_FAILED);
      else
	if(pmStatus!=NULL  && strncasecmp(pmStatus->value,"Succeeded",9)==0)
	  pjoRes=createStatus(pmsConf,SERVICE_SUCCEEDED);
	else
	  if(pmStatus!=NULL  && strncasecmp(pmStatus->value,"Running",7)==0){
	    /*map* tmpMap=getMapFromMaps(pmsConf,"lenv","Message");
	    if(tmpMap!=NULL)
	      setMapInMaps(pmsConf,"lenv","gs_message",tmpMap->value);*/
	    pjoRes=createStatus(pmsConf,SERVICE_STARTED);
	  }
	  else
	    pjoRes=createStatus(pmsConf,SERVICE_FAILED);
    }
    return pjoRes;
  }
  
  /**
   * Print the jobs list
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @return the JSON object pointer to the jobs list
   */
  json_object* printJobList(maps* conf){
    json_object* res=json_object_new_array();
    map* pmJobs=getMapFromMaps(conf,"lenv","selectedJob");
    map* tmpPath=getMapFromMaps(conf,"main","tmpPath");
    struct dirent *dp;
    int cnt=0;
    int skip=0;
    int limit=10000;
    map* pmLimit=getMapFromMaps(conf,"lenv","serviceCntLimit");
    map* pmSkip=getMapFromMaps(conf,"lenv","serviceCntSkip");
    if(pmLimit!=NULL){
      limit=atoi(pmLimit->value);
    }
    if(pmSkip!=NULL){
      skip=atoi(pmSkip->value);
    }
    if(pmJobs!=NULL){
      char *saveptr;
      char *tmps = strtok_r(pmJobs->value, ",", &saveptr);
      while(tmps!=NULL){
	if(cnt>=skip && cnt<limit+skip && strlen(tmps)>2){
	  char* tmpStr=zStrdup(tmps+1);
	  tmpStr[strlen(tmpStr)-1]=0;
	  json_object* cjob=printJobStatus(conf,tmpStr);
	  json_object_array_add(res,cjob);
	  free(tmpStr);
	}
	if(cnt==limit+skip)
	  setMapInMaps(conf,"lenv","serviceCntNext","true");
	cnt++;
	tmps = strtok_r (NULL, ",", &saveptr);
      }
    }else{
      DIR *dirp = opendir (tmpPath->value);
      if(dirp!=NULL){
	while ((dp = readdir (dirp)) != NULL){
	  char* extn = strstr(dp->d_name, "_status.json");
	  if(extn!=NULL){
	    if(cnt>=skip && cnt<limit+skip){
	      char* tmpStr=zStrdup(dp->d_name);
	      tmpStr[strlen(dp->d_name)-12]=0;
	      json_object* cjob=printJobStatus(conf,tmpStr);
	      json_object_array_add(res,cjob);
	    }
	    if(cnt==limit+skip)
	      setMapInMaps(conf,"lenv","serviceCntNext","true");
	    cnt++;
	  }
	}
	closedir (dirp);
      }
    }
    json_object* resFinal=json_object_new_object();
    json_object_object_add(resFinal,"jobs",res);
    setMapInMaps(conf,"lenv","path","jobs");
    char pcCnt[10];
    sprintf(pcCnt,"%d",cnt);
    setMapInMaps(conf,"lenv","serviceCnt",pcCnt);
    createNextLinks(conf,resFinal);
    return resFinal;
  }
  
  /**
   * Print the result of an execution
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param s service pointer to metadata
   * @param result outputs of the service
   * @param res the status of execution SERVICE_FAILED/SERVICE_SUCCEEDED
   * @return the JSON object pointer to the result
   */
  json_object* printJResult(maps* conf,service* s,maps* result,int res){
    if(res==SERVICE_FAILED){
      char* pacTmp=produceErrorMessage(conf);
      map* pamTmp=createMap("message",pacTmp);
      free(pacTmp);
      map* pmTmp=getMapFromMaps(conf,"lenv","code");
      if(pmTmp!=NULL)
	addToMap(pamTmp,"code",pmTmp->value);
      printExceptionReportResponseJ(conf,pamTmp);
      freeMap(&pamTmp);
      free(pamTmp);
      return NULL;
    }
    if(getMapFromMaps(conf,"lenv","no-headers")==NULL)
      printHeaders(conf);
    map* pmMode=getMapFromMaps(conf,"request","response");
    if(pmMode!=NULL && strncasecmp(pmMode->value,"raw",3)==0){
      maps* resu=result;
      printRawdataOutputs(conf,s,resu);
      map* pmError=getMapFromMaps(conf,"lenv","error");
      if(pmError!=NULL && strncasecmp(pmError->value,"true",4)==0){
	printExceptionReportResponseJ(conf,pmError);
      }
      return NULL;
    }else{
      json_object* eres1=json_object_new_object();
      map* pmIOAsArray=getMapFromMaps(conf,"openapi","io_as_array");
      json_object* eres;
      if(pmIOAsArray!=NULL && strncasecmp(pmIOAsArray->value,"true",4)==0)
	eres=json_object_new_array();
      else
	eres=json_object_new_object();
      maps* resu=result;
      int itn=0;
      while(resu!=NULL){
	json_object* res1=json_object_new_object();
	if(pmIOAsArray!=NULL && strncasecmp(pmIOAsArray->value,"true",4)==0)
	  json_object_object_add(res1,"id",json_object_new_string(resu->name));
	map* tmpMap=getMap(resu->content,"mimeType");
	json_object* res3=json_object_new_object();
	if(tmpMap!=NULL)
	  json_object_object_add(res3,"mediaType",json_object_new_string(tmpMap->value));
	else{
	  json_object_object_add(res3,"mediaType",json_object_new_string("text/plain"));
	}
	map* pmTransmissionMode=NULL;
	map* pmPresence=getMap(resu->content,"inRequest");
	if(((tmpMap=getMap(resu->content,"value"))!=NULL ||
	   (getMap(resu->content,"generated_file"))!=NULL) &&
	   (pmPresence!=NULL && strncmp(pmPresence->value,"true",4)==0)){
	  map* tmpMap1=NULL;
	  if((tmpMap1=getMap(resu->content,"transmissionMode"))!=NULL) {
	    pmTransmissionMode=getMap(resu->content,"transmissionMode");
	    if(strcmp(tmpMap1->value,"value")==0) {
	      map* tmpMap2=getMap(resu->content,"mimeType");
	      if(tmpMap2!=NULL && strstr(tmpMap2->value,"json")!=NULL){
		json_object *jobj = NULL;
		map *gfile=getMap(resu->content,"generated_file");
		if(gfile!=NULL){
		  gfile=getMap(resu->content,"expected_generated_file");
		  if(gfile==NULL){
		    gfile=getMap(resu->content,"generated_file");
		  }
		  jobj=json_readFile(conf,gfile->value);
		}
		else{
		  int slen = 0;
		  enum json_tokener_error jerr;
		  struct json_tokener* tok=json_tokener_new();
		  do {
		    slen = strlen(tmpMap->value);
		    jobj = json_tokener_parse_ex(tok, tmpMap->value, slen);
		  } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
		  if (jerr != json_tokener_success) {
		    fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
		    return eres1;
		  }
		  if (tok->char_offset < slen){
		    return eres1;
		  }
		}
		json_object_object_add(res3,"encoding",json_object_new_string("utf-8"));
		json_object_object_add(res1,"value",jobj);
	      }else{
		map* tmp1=getMapFromMaps(conf,"main","tmpPath");
		map *gfile=getMap(resu->content,"generated_file");
		if(gfile!=NULL){
		  gfile=getMap(resu->content,"expected_generated_file");
		  if(gfile==NULL){
		    gfile=getMap(resu->content,"generated_file");
		  }
		  FILE* pfData=fopen(gfile->value,"rb");
		  if(pfData!=NULL){
		    zStatStruct f_status;
		    int s=zStat(gfile->value, &f_status);
		    char* pcaTmp=(char*)malloc((f_status.st_size+1)*sizeof(char));
		    fread(pcaTmp,1,f_status.st_size,pfData);
		    pcaTmp[f_status.st_size]=0;
		    fclose(pfData);
		    map* pmMediaType=getMap(resu->content,"mediaType");
		    if(pmMediaType!=NULL){
		      if(strstr(pmMediaType->value,"json")!=NULL || strstr(pmMediaType->value,"text")!=NULL || strstr(pmMediaType->value,"kml")!=NULL){
			if(strstr(pmMediaType->value,"json")!=NULL){
			  json_object* pjoaTmp=parseJson(conf,pcaTmp);
			  json_object_object_add(res1,"value",pjoaTmp);
			}
			else
			  json_object_object_add(res1,"value",json_object_new_string(pcaTmp));
		      }
		    }else{
		      json_object_object_add(res1,"value",json_object_new_string(base64(pcaTmp,f_status.st_size)));
		      json_object_object_add(res3,"encoding",json_object_new_string("base64"));
		    }
		    free(pcaTmp);
		  }
		}else{
		  json_object_object_add(res3,"encoding",json_object_new_string("utf-8"));
		  map* tmpMap0=getMap(resu->content,"crs");
		  if(tmpMap0!=NULL){
		    map* tmpMap2=getMap(resu->content,"value");
		    json_object *jobj = NULL;
		    const char *mystring = NULL;
		    int slen = 0;
		    enum json_tokener_error jerr;
		    struct json_tokener* tok=json_tokener_new();
		    do {
		      mystring = tmpMap2->value;  // get JSON string, e.g. read from file, etc...
		      slen = strlen(mystring);
		      jobj = json_tokener_parse_ex(tok, mystring, slen);
		    } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
		    if (jerr != json_tokener_success) {
		      map* pamError=createMap("code","InvalidParameterValue");
		      const char* pcTmpErr=json_tokener_error_desc(jerr);
		      const char* pccErr=_("ZOO-Kernel cannot parse your POST data: %s");
		      char* pacMessage=(char*)malloc((strlen(pcTmpErr)+strlen(pccErr)+1)*sizeof(char));
		      sprintf(pacMessage,pccErr,pcTmpErr);
		      addToMap(pamError,"message",pacMessage);
		      printExceptionReportResponseJ(conf,pamError);
		      fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
		      return NULL;
		    }
		    if (tok->char_offset < slen){
		      map* pamError=createMap("code","InvalidParameterValue");
		      const char* pcTmpErr="None";
		      const char* pccErr=_("ZOO-Kernel cannot parse your POST data: %s");
		      char* pacMessage=(char*)malloc((strlen(pcTmpErr)+strlen(pccErr)+1)*sizeof(char));
		      sprintf(pacMessage,pccErr,pcTmpErr);
		      addToMap(pamError,"message",pacMessage);
		      printExceptionReportResponseJ(conf,pamError);
		      fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
		      return NULL;
		    }
		    json_object_object_add(res1,"bbox",jobj);
		    json_object_object_add(res1,"crs",json_object_new_string(tmpMap0->value));
		  }
		  else
		    json_object_object_add(res1,"value",json_object_new_string(tmpMap->value));
		}
		map* tmpMap0=getMap(resu->content,"crs");
		if(tmpMap0==NULL){
		  json_object_object_foreach(res3, key, val) {
		    if(strncasecmp(key,pccFields[0],4)==0 ||
		       strncasecmp(key,pccFields[3],8)==0 ||
		       strncasecmp(key,pccFields[6],15)==0)
		      json_object_object_add(res1,pccFields[3],json_object_new_string(json_object_get_string(val)));
		    if(strncasecmp(key,pccFields[1],4)==0 ||
		       strncasecmp(key,pccFields[4],8)==0 ||
		       strncasecmp(key,pccFields[7],15)==0)
		      json_object_object_add(res1,pccFields[4],json_object_new_string(json_object_get_string(val)));
		    if(strncasecmp(key,pccFields[2],4)==0 ||
		       strncasecmp(key,pccFields[5],8)==0 ||
		       strncasecmp(key,pccFields[8],15)==0)
		      json_object_object_add(res1,pccFields[5],json_object_new_string(json_object_get_string(val)));
		  }
		}
	      }
	    }
	    else{
	      map* pmTest=getMap(resu->content,"useMapserver");
	      if(pmTest!=NULL){
		map* geodatatype=getMap(resu->content,"geodatatype");
		map* nbFeatures;
		setMapInMaps(conf,"lenv","state","out");
		setReferenceUrl(conf,resu);
		nbFeatures=getMap(resu->content,"nb_features");
		geodatatype=getMap(resu->content,"geodatatype");
		if((nbFeatures!=NULL && atoi(nbFeatures->value)==0) ||
		   (geodatatype!=NULL && strcasecmp(geodatatype->value,"other")==0)){
		  //error=1;
		  //res=SERVICE_FAILED;
		}else{
		  map* pmUrl=getMap(resu->content,"Reference");
		  json_object_object_add(res1,"href",
					 json_object_new_string(pmUrl->value));
		  map* pmMimeType=getMap(resu->content,"requestedMimeType");
		  if(pmMimeType!=NULL)
		    json_object_object_add(res1,pccFields[0],json_object_new_string(pmMimeType->value));
		}
	      }else{
		char *pcaFileUrl=produceFileUrl(s,conf,resu,NULL,itn);
		itn++;
		if(pcaFileUrl!=NULL){
		  json_object_object_add(res1,"href",
					 json_object_new_string(pcaFileUrl));
		  free(pcaFileUrl);
		}
		json_object_object_foreach(res3, key, val) {
		  if(strncasecmp(key,pccFields[0],4)==0 ||
		     strncasecmp(key,pccFields[3],8)==0 ||
		     strncasecmp(key,pccFields[6],15)==0)
		    json_object_object_add(res1,pccFields[0],json_object_new_string(json_object_get_string(val)));
		}
	      }
	    }
	  }
	  json_object_put(res3);
	  if(pmIOAsArray!=NULL && strncasecmp(pmIOAsArray->value,"true",4)==0){
	    json_object_array_add(eres,res1);
	  }else{
	    map* pmDataType=getMap(resu->content,"dataType");
	    if(pmDataType==NULL || (pmTransmissionMode!=NULL && strncasecmp(pmTransmissionMode->value,"reference",9)==0) )
	      json_object_object_add(eres,resu->name,res1);
	    else{
	      if(((tmpMap=getMap(resu->content,"value"))!=NULL))
		json_object_object_add(eres,resu->name,json_object_new_string(tmpMap->value));		
	    }
	  }
	}
	resu=resu->next;
      }
      const char* jsonStr =
	json_object_to_json_string_ext(eres,JSON_C_TO_STRING_NOSLASHESCAPE);
      map *tmpPath = getMapFromMaps (conf, "main", "tmpPath");
      map *cIdentifier = getMapFromMaps (conf, "lenv", "oIdentifier");
      map *sessId = getMapFromMaps (conf, "lenv", "usid");
      char *pacTmp=(char*)malloc((strlen(tmpPath->value)+strlen(cIdentifier->value)+strlen(sessId->value)+8)*sizeof(char));
      sprintf(pacTmp,"%s/%s_%s.json",
	      tmpPath->value,cIdentifier->value,sessId->value);
      zStatStruct zsFStatus;
      int iS=zStat(pacTmp, &zsFStatus);
      if(iS==0 && zsFStatus.st_size>0){
	map* tmpPath1 = getMapFromMaps (conf, "main", "tmpUrl");
	char* pacTmpUrl=(char*)malloc((strlen(tmpPath1->value)+strlen(cIdentifier->value)+strlen(sessId->value)+8)*sizeof(char));;
	sprintf(pacTmpUrl,"%s/%s_%s.json",tmpPath1->value,
		cIdentifier->value,sessId->value);
	if(getMapFromMaps(conf,"lenv","gs_location")==NULL)
	  setMapInMaps(conf,"headers","Location",pacTmpUrl);
	free(pacTmpUrl);
      }
      free(pacTmp);
      if(res==3){
	map* mode=getMapFromMaps(conf,"request","mode");
	if(mode!=NULL && strncasecmp(mode->value,"async",5)==0)
	  setMapInMaps(conf,"headers","Status","201 Created");
	else
	  setMapInMaps(conf,"headers","Status","200 Ok");
      }
      else{
	setMapInMaps(conf,"headers","Status","500 Issue running your service");
      }
      return eres;
    }
  }


  /**
   * Create a link object with ref, type and href.
   *
   * @param conf maps pointer to the main configuration maps
   * @param rel char pointer defining the rel attribute
   * @param type char pointer defining the type attribute
   * @param href char pointer defining the href attribute
   * @return json_object pointer to the link object
   */
  json_object* createALink(maps* conf,const char* rel,const char* type,const char* href){
    json_object* val=json_object_new_object();
    json_object_object_add(val,"rel",
			   json_object_new_string(rel));
    json_object_object_add(val,"type",
			   json_object_new_string(type));
    json_object_object_add(val,"href",json_object_new_string(href));
    return val;
  }

  /**
   * Create the next links
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param result an integer (>0 for adding the /result link)
   * @param obj the JSON object pointer to add the links to
   * @return 0
   */
  int createNextLinks(maps* conf,json_object* obj){
    json_object* res=json_object_new_array();

    map *tmpPath = getMapFromMaps (conf, "openapi", "rootUrl");
    map *sessId = getMapFromMaps (conf, "lenv", "path");

    char *Url0=(char*) malloc((strlen(tmpPath->value)+
			       strlen(sessId->value)+2)*sizeof(char));
    sprintf(Url0,"%s/%s",
	    tmpPath->value,
	    sessId->value);
    json_object_array_add(res,createALink(conf,"self","application/json",Url0));

    map* pmHtml=getMapFromMaps(conf,"openapi","partial_html_support");
    if(pmHtml!=NULL && strcmp(pmHtml->value,"true")==0){
      char *Url1=(char*) malloc((strlen(tmpPath->value)+
				 strlen(sessId->value)+7)*sizeof(char));
      sprintf(Url1,"%s/%s.html",
	      tmpPath->value,
	      sessId->value);
      json_object_array_add(res,createALink(conf,"alternate","text/html",Url1));
      free(Url1);
    }

    map* pmTmp=getMapFromMaps(conf,"lenv","serviceCntSkip");
    map* pmLimit=getMapFromMaps(conf,"lenv","serviceCntLimit");
    map* pmCnt=getMapFromMaps(conf,"lenv","serviceCnt");
    if(pmTmp!=NULL && atoi(pmTmp->value)>0){
      char* pcaTmp=(char*) malloc((strlen(Url0)+strlen(pmLimit->value)+25)*sizeof(char));
      int cnt=atoi(pmTmp->value)-atoi(pmLimit->value);
      if(cnt>=0)
	sprintf(pcaTmp,"%s?limit=%s&skip=%d",Url0,pmLimit->value,cnt);
      else
	sprintf(pcaTmp,"%s?limit=%s&skip=%d",Url0,pmLimit->value,0);
      json_object_array_add(res,createALink(conf,"prev","application/json",pcaTmp));
      free(pcaTmp);
    }
    if(getMapFromMaps(conf,"lenv","serviceCntNext")!=NULL){
      char* pcaTmp=(char*) malloc((strlen(Url0)+strlen(pmLimit->value)+strlen(pmCnt->value)+15)*sizeof(char));
      int cnt=(pmTmp!=NULL?atoi(pmTmp->value):0)+atoi(pmLimit->value);
      sprintf(pcaTmp,"%s?limit=%s&skip=%d",Url0,pmLimit->value,cnt);
      json_object_array_add(res,createALink(conf,"next","application/json",pcaTmp));
      free(pcaTmp);
    }
    json_object_object_add(obj,"links",res);
    free(Url0);
    map* pmNb=getMapFromMaps(conf,"lenv","serviceCnt");
    if(pmNb!=NULL)
      json_object_object_add(obj,"numberTotal",json_object_new_int(atoi(pmNb->value)));

    return 0;
  }

  /**
   * Create the status links
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param result an integer (>0 for adding the /result link)
   * @param obj the JSON object pointer to add the links to
   * @return 0
   */
  int createStatusLinks(maps* conf,int result,json_object* obj){
    json_object* res=json_object_new_array();
    map *tmpPath = getMapFromMaps (conf, "openapi", "rootUrl");
    map *sessId = getMapFromMaps (conf, "lenv", "usid");
    if(sessId==NULL){
      sessId = getMapFromMaps (conf, "lenv", "gs_usid");
    }
    char *Url0=(char*) malloc((strlen(tmpPath->value)+
			       strlen(sessId->value)+18)*sizeof(char));
    int needResult=-1;
    char *message, *status;
    sprintf(Url0,"%s/jobs/%s",
	    tmpPath->value,
	    sessId->value);
    if(getMapFromMaps(conf,"lenv","gs_location")==NULL)
      setMapInMaps(conf,"headers","Location",Url0);
    json_object* val=json_object_new_object();
    json_object_object_add(val,"title",
			   json_object_new_string(_("Status location")));
    json_object_object_add(val,"rel",
			   json_object_new_string(_("status")));
    json_object_object_add(val,"type",
			   json_object_new_string(_("application/json")));
    json_object_object_add(val,"href",json_object_new_string(Url0));
    json_object_array_add(res,val);
    if(result>0){
      free(Url0);
      Url0=(char*) malloc((strlen(tmpPath->value)+
			   strlen(sessId->value)+
			   25)*sizeof(char));
      sprintf(Url0,"%s/jobs/%s/results",
	      tmpPath->value,
	      sessId->value);
      json_object* val1=json_object_new_object();
      json_object_object_add(val1,"title",
			     json_object_new_string(_("Result location")));
      json_object_object_add(val1,"rel",
			     json_object_new_string(_("results")));
      json_object_object_add(val1,"type",
			     json_object_new_string(_("application/json")));
      json_object_object_add(val1,"href",json_object_new_string(Url0));
      json_object_array_add(res,val1);
    }
    json_object_object_add(obj,"links",res);
    free(Url0);
    return 0;
  }

  /**
   * Get the status file path
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @return a char* containing the full status file path
   */
  char* json_getStatusFilePath(maps* conf){
    map *tmpPath = getMapFromMaps (conf, "main", "tmpPath");
    map *sessId = getMapFromMaps (conf, "lenv", "usid");
    if(sessId!=NULL){
      sessId = getMapFromMaps (conf, "lenv", "gs_usid");
      if(sessId==NULL)
	sessId = getMapFromMaps (conf, "lenv", "usid");
    }else
      sessId = getMapFromMaps (conf, "lenv", "gs_usid");
    
    char *tmp1=(char*) malloc((strlen(tmpPath->value)+
			       strlen(sessId->value)+14)*sizeof(char));
    sprintf(tmp1,"%s/%s_status.json",
	    tmpPath->value,
	    sessId->value);
    return tmp1;
  }

  /**
   * Get the result path
   *
   * @param conf the conf maps containing the main.cfg settings
   * @param jobId the job identifier
   * @return the full path to the result file
   */
  char* getResultPath(maps* conf,char* jobId){
    map *tmpPath = getMapFromMaps (conf, "main", "tmpPath");
    char *pacUrl=(char*) malloc((strlen(tmpPath->value)+
				 strlen(jobId)+8)*sizeof(char));
    sprintf(pacUrl,"%s/%s.json",tmpPath->value,jobId);
    return pacUrl;
  }

  /**
   * Parse a json string
   *
   * @param conf the conf maps containing the main.cfg settings
   * @param myString the string containing the json content
   * @return a pointer to the created json_object
   */
  json_object* parseJson(maps* conf,char* myString){
    json_object *pajObj = NULL;
    enum json_tokener_error jerr;
    struct json_tokener* tok=json_tokener_new();
    int slen = 0;
    do {
      slen = strlen(myString);
      pajObj = json_tokener_parse_ex(tok, myString, slen);
    } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
    if (jerr != json_tokener_success) {
      setMapInMaps(conf,"lenv","message",json_tokener_error_desc(jerr));
      fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
      return NULL;
    }
    if (tok->char_offset < slen){
      fprintf(stderr, "Error parsing json\n");
      return NULL;
    }
    return pajObj;
  }

  /**
   * Read a json file
   *
   * @param conf the conf maps containing the main.cfg settings
   * @praam filePath the file path to read
   * @return a pointer to the created json_object
   */
  json_object* json_readFile(maps* conf,char* filePath){
    json_object *pajObj = NULL;
    zStatStruct zsFStatus;
    int iS=zStat(filePath, &zsFStatus);
    if(iS==0 && zsFStatus.st_size>0){
      FILE* cdat=fopen(filePath,"rb");
      if(cdat!=NULL){
	char* pacMyString=(char*)malloc((zsFStatus.st_size+1)*sizeof(char));
	fread(pacMyString,1,zsFStatus.st_size,cdat);
	pacMyString[zsFStatus.st_size]=0;
	fclose(cdat);
	pajObj=parseJson(conf,pacMyString);
	free(pacMyString);
      }
      else
	return NULL;
    }else
      return NULL;
    return pajObj;
  }

  /**
   * Create the json object for job status
   *
   * @param conf the conf maps containing the main.cfg settings
   * @param status integer
   * @return the created json_object
   */
  json_object* createStatus(maps* conf,int status){
    int hasMessage=0;
    int needResult=0;
    int isDismissed=0;
    const char *rstatus;
    char *message;
    // Create statusInfo JSON object
    // cf. https://github.com/opengeospatial/wps-rest-binding/blob/master/core/
    //     openapi/schemas/statusInfo.yaml
    json_object* res=json_object_new_object();
    switch(status){
    case SERVICE_ACCEPTED:
      {
	message=_("ZOO-Kernel accepted to run your service!");
	rstatus="accepted";
	break;
      }
    case SERVICE_STARTED:
      {
	message=_("ZOO-Kernel is currently running your service!");
	map* pmStatus=getMapFromMaps(conf,"lenv","message");
	if(pmStatus!=NULL){
	  setMapInMaps(conf,"lenv","gs_message",pmStatus->value);
	  message=zStrdup(pmStatus->value);
	  hasMessage=1;
	}
	rstatus="running";
	break;
      }
    case SERVICE_PAUSED:
      {
	message=_("ZOO-Kernel pause your service!");
	rstatus="paused";
	break;
      }
    case SERVICE_SUCCEEDED:
      {
	message=_("ZOO-Kernel successfully run your service!");
	rstatus="successful";
	setMapInMaps(conf,"lenv","PercentCompleted","100");
	needResult=1;
	break;
      }
    case SERVICE_DISMISSED:
      {
	message=_("ZOO-Kernel successfully dismissed your service!");
	rstatus="dismissed";
	needResult=1;
	isDismissed=1;
	break;
      }
    default:
      {
	map* pmTmp=getMapFromMaps(conf,"lenv","force");
	if(pmTmp==NULL || strncasecmp(pmTmp->value,"false",5)==0){
	  char* pacTmp=json_getStatusFilePath(conf);
	  json_object* pjoStatus=json_readFile(conf,pacTmp);
	  free(pacTmp);
	  if(pjoStatus!=NULL){
	    json_object* pjoMessage=NULL;
	    if(json_object_object_get_ex(pjoStatus,"message",&pjoMessage)!=FALSE){
	      message=(char*)json_object_get_string(pjoMessage);
	    }
	  }
	  // TODO: Error
	}else{
	  map* mMap=getMapFromMaps(conf,"lenv","gs_message");
	  if(mMap!=NULL)
	    setMapInMaps(conf,"lenv","message",mMap->value);
	  message=produceErrorMessage(conf);
	  hasMessage=1;
	  needResult=-1;
	}
	rstatus="failed";
	break;
      }
    }
    setMapInMaps(conf,"lenv","message",message);
    setMapInMaps(conf,"lenv","status",rstatus);

    map *sessId = getMapFromMaps (conf, "lenv", "usid");
    if(sessId!=NULL){
      sessId = getMapFromMaps (conf, "lenv", "gs_usid");
      if(sessId==NULL)
	sessId = getMapFromMaps (conf, "lenv", "usid");
    }else
      sessId = getMapFromMaps (conf, "lenv", "gs_usid");
    if(sessId!=NULL && isDismissed==0){
      json_object_object_add(res,"jobID",json_object_new_string(sessId->value));
      for(int i=0;i<5;i++){
	char* pcaTmp=_getStatusField(conf,sessId->value,statusFieldsC[i]);
	if(pcaTmp!=NULL && strcmp(pcaTmp,"-1")!=0){
	  json_object_object_add(res,statusFields[i],json_object_new_string(pcaTmp));
	}
	if(pcaTmp!=NULL && strncmp(pcaTmp,"-1",2)==0)
	  free(pcaTmp);
      }
    }
    json_object_object_add(res,"status",json_object_new_string(rstatus));
    map* mMap=NULL;
    /* if(mMap==NULL)*/
    json_object_object_add(res,"message",json_object_new_string(message));
      /*else{
      json_object_object_add(res,"message",json_object_new_string(mMap->value));
      }*/
    if((mMap=getMapFromMaps(conf,"lenv","PercentCompleted"))!=NULL)
      json_object_object_add(res,"progress",json_object_new_int(atoi(mMap->value)));
    if(status!=SERVICE_DISMISSED)
      createStatusLinks(conf,needResult,res);
    else{
      json_object* res1=json_object_new_array();
      map *tmpPath = getMapFromMaps (conf, "openapi", "rootUrl");
      char *Url0=(char*) malloc((strlen(tmpPath->value)+17)*sizeof(char));
      sprintf(Url0,"%s/jobs",tmpPath->value);
      json_object* val=json_object_new_object();
      json_object_object_add(val,"title",
			     json_object_new_string(_("The job list for the current process")));
      json_object_object_add(val,"rel",
			     json_object_new_string(_("parent")));
      json_object_object_add(val,"type",
			     json_object_new_string(_("application/json")));
      json_object_object_add(val,"href",json_object_new_string(Url0));
      json_object_array_add(res1,val);
      free(Url0);
      json_object_object_add(res,"links",res1);
    }
    if(hasMessage>0)
      free(message);
    return res;
  }
  
  /**
   * Create the status file 
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param status an integer (SERVICE_ACCEPTED,SERVICE_STARTED...)
   * @return an integer (0 in case of success, 1 in case of failure)
   */
  int createStatusFile(maps* conf,int status){
    json_object* res=createStatus(conf,status);
    char* tmp1=json_getStatusFilePath(conf);
    FILE* foutput1=fopen(tmp1,"w+");
    if(foutput1!=NULL){
      const char* jsonStr1=json_object_to_json_string_ext(res,JSON_C_TO_STRING_NOSLASHESCAPE);
      fprintf(foutput1,"%s",jsonStr1);
      fclose(foutput1);
    }else{
      // Failure
      setMapInMaps(conf,"lenv","message",_("Unable to store the statusInfo!"));
      free(tmp1);
      return 1;
    }
    free(tmp1);
    return 0;
  }

  /**
   * Create the status file 
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @return an integer (0 in case of success, 1 in case of failure)
   */
  int json_getStatusFile(maps* conf){
    char* tmp1=json_getStatusFilePath(conf);
    FILE* statusFile=fopen(tmp1,"rb");

    map* tmpMap=getMapFromMaps(conf,"lenv","gs_usid");
    if(tmpMap!=NULL){
      char* tmpStr=_getStatus(conf,tmpMap->value);
      if(tmpStr!=NULL && strncmp(tmpStr,"-1",2)!=0){
	char *tmpStr1=zStrdup(tmpStr);
	char *tmpStr0=zStrdup(strstr(tmpStr,"|")+1);
	free(tmpStr);
	tmpStr1[strlen(tmpStr1)-strlen(tmpStr0)-1]='\0';
	setMapInMaps(conf,"lenv","PercentCompleted",tmpStr1);
	setMapInMaps(conf,"lenv","gs_message",tmpStr0);
	free(tmpStr0);
	free(tmpStr1);
	return 0;
      }else{
	  
      }
    }
  }

  /**
   * Produce the JSON object for api info object
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param res the JSON object for the api info
   */
  void produceApiInfo(maps* conf,json_object* res,json_object* res5){
    json_object *res1=json_object_new_object();
    map* tmpMap=getMapFromMaps(conf,"provider","providerName");
    if(tmpMap!=NULL){
      json_object *res2=json_object_new_object();
      json_object_object_add(res2,"name",json_object_new_string(tmpMap->value));
      tmpMap=getMapFromMaps(conf,"provider","addressElectronicMailAddress");
      if(tmpMap!=NULL)
	json_object_object_add(res2,"email",json_object_new_string(tmpMap->value));
      tmpMap=getMapFromMaps(conf,"provider","providerSite");
      if(tmpMap!=NULL)
	json_object_object_add(res2,"url",json_object_new_string(tmpMap->value));
      json_object_object_add(res1,"contact",res2);
    }
    tmpMap=getMapFromMaps(conf,"identification","abstract");
    if(tmpMap!=NULL){
      json_object_object_add(res1,"description",json_object_new_string(tmpMap->value));
      json_object_object_add(res5,"description",json_object_new_string(tmpMap->value));
      tmpMap=getMapFromMaps(conf,"openapi","rootUrl");
      json_object_object_add(res5,"url",json_object_new_string(tmpMap->value));
    }
    tmpMap=getMapFromMaps(conf,"identification","title");
    if(tmpMap!=NULL)
      json_object_object_add(res1,"title",json_object_new_string(tmpMap->value));
    json_object_object_add(res1,"version",json_object_new_string(ZOO_VERSION));
    tmpMap=getMapFromMaps(conf,"identification","keywords");
    if(tmpMap!=NULL){
      char *saveptr;
      char *tmps = strtok_r (tmpMap->value, ",", &saveptr);
      json_object *res3=json_object_new_array();
      while (tmps != NULL){
	json_object_array_add(res3,json_object_new_string(tmps));
	tmps = strtok_r (NULL, ",", &saveptr);
      }
      json_object_object_add(res1,"x-keywords",res3);
    }
    maps* tmpMaps=getMaps(conf,"provider");
    if(tmpMaps!=NULL){
      json_object *res4=json_object_new_object();
      map* cItem=tmpMaps->content;
      while(cItem!=NULL){
	json_object_object_add(res4,cItem->name,json_object_new_string(cItem->value));
	cItem=cItem->next;
      }
      json_object_object_add(res1,"x-ows-servicecontact",res4);
    }

    json_object *res4=json_object_new_object();
    tmpMap=getMapFromMaps(conf,"openapi","license_name");
    if(tmpMap!=NULL){
      json_object_object_add(res4,"name",json_object_new_string(tmpMap->value));
      tmpMap=getMapFromMaps(conf,"openapi","license_url");
      if(tmpMap!=NULL){
	json_object_object_add(res4,"url",json_object_new_string(tmpMap->value));      
      }
    }
    json_object_object_add(res1,"license",res4);

    json_object_object_add(res,"info",res1);    
  }

  // addResponse(pmUseContent,cc3,vMap,tMap,"200","successful operation");
  void addResponse(const map* useContent,json_object* res,const map* pmSchema,const map* pmType,const char* code,const char* msg){
    json_object *cc=json_object_new_object();
    if(pmSchema!=NULL)
      json_object_object_add(cc,"$ref",json_object_new_string(pmSchema->value));
    if(useContent!=NULL && strncasecmp(useContent->value,"true",4)!=0){
      json_object_object_add(res,code,cc);
    }else{
      json_object *cc0=json_object_new_object();
      if(pmSchema!=NULL)
	json_object_object_add(cc0,"schema",cc);
      json_object *cc1=json_object_new_object();
      if(pmType!=NULL)
	json_object_object_add(cc1,pmType->value,cc0);
      else
	json_object_object_add(cc1,"application/json",cc0);
      json_object *cc2=json_object_new_object();
      json_object_object_add(cc2,"content",cc1);
      json_object_object_add(cc2,"description",json_object_new_string(msg));
      json_object_object_add(res,code,cc2);
    }
  }

  void addParameter(maps* conf,const char* oName,const char* fName,const char* in,json_object* res){
    maps* tmpMaps1=getMaps(conf,oName);
    json_object *res8=json_object_new_object();
    if(tmpMaps1!=NULL){
      map* tmpMap=getMap(tmpMaps1->content,"title");
      if(tmpMap!=NULL)
	json_object_object_add(res8,"x-internal-summary",json_object_new_string(tmpMap->value));
      tmpMap=getMap(tmpMaps1->content,"schema");
      if(tmpMap!=NULL)
	json_object_object_add(res8,"$ref",json_object_new_string(tmpMap->value));
      tmpMap=getMap(tmpMaps1->content,"abstract");
      if(tmpMap!=NULL)
	json_object_object_add(res8,"description",json_object_new_string(tmpMap->value));
      tmpMap=getMap(tmpMaps1->content,"example");
      if(tmpMap!=NULL)
	json_object_object_add(res8,"example",json_object_new_string(tmpMap->value));
      tmpMap=getMap(tmpMaps1->content,"required");
      if(tmpMap!=NULL){
	if(strcmp(tmpMap->value,"true")==0)
	  json_object_object_add(res8,"required",json_object_new_boolean(TRUE));
	else
	  json_object_object_add(res8,"required",json_object_new_boolean(FALSE));
      }
      else
	json_object_object_add(res8,"required",json_object_new_boolean(FALSE));
      map* pmTmp=getMap(tmpMaps1->content,"in");
      if(pmTmp!=NULL)
	json_object_object_add(res8,"in",json_object_new_string(pmTmp->value));
      else
	json_object_object_add(res8,"in",json_object_new_string(in));
      tmpMap=getMap(tmpMaps1->content,"name");
      if(tmpMap!=NULL)
	json_object_object_add(res8,"name",json_object_new_string(tmpMap->value));
      else
	json_object_object_add(res8,"name",json_object_new_string(fName));
      tmpMap=getMap(tmpMaps1->content,"schema");
      if(tmpMap==NULL){
	json_object *res6=json_object_new_object();
	tmpMap=getMap(tmpMaps1->content,"type");
	char* pcaType=NULL;
	if(tmpMap!=NULL){
	  json_object_object_add(res6,"type",json_object_new_string(tmpMap->value));
	  pcaType=zStrdup(tmpMap->value);
	}
	else
	  json_object_object_add(res6,"type",json_object_new_string("string"));
      
	tmpMap=getMap(tmpMaps1->content,"enum");
	if(tmpMap!=NULL){
	  char *saveptr12;
	  char *tmps12 = strtok_r (tmpMap->value, ",", &saveptr12);
	  json_object *pjoEnum=json_object_new_array();
	  while(tmps12!=NULL){
	    json_object_array_add(pjoEnum,json_object_new_string(tmps12));
	    tmps12 = strtok_r (NULL, ",", &saveptr12);
	  }
	  json_object_object_add(res6,"enum",pjoEnum);
	}

	pmTmp=tmpMaps1->content;
	while(pmTmp!=NULL){
	  if(strstr(pmTmp->name,"schema_")!=NULL){
	    if(pcaType!=NULL && strncmp(pcaType,"integer",7)==0){
	      json_object_object_add(res6,strstr(pmTmp->name,"schema_")+7,json_object_new_int(atoi(pmTmp->value)));
	    }else
	      json_object_object_add(res6,strstr(pmTmp->name,"schema_")+7,json_object_new_string(pmTmp->value));
	  }
	  pmTmp=pmTmp->next;
	}
	if(pcaType!=NULL)
	  free(pcaType);
	json_object_object_add(res8,"schema",res6);
      }
    }
    json_object_object_add(res,fName,res8);    
  }
  
  /**
   * Produce the JSON object for api parameter
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param res the JSON object to populate with the parameters
   */
  void produceApiParameters(maps* conf,json_object* res){
    json_object *res9=json_object_new_object();
    maps* tmpMaps1=getMaps(conf,"{id}");
    map* tmpMap2=getMapFromMaps(conf,"openapi","parameters");
    char *saveptr12;
    char *tmps12 = strtok_r (tmpMap2->value, ",", &saveptr12);
    while(tmps12!=NULL){
      char* pacId=(char*) malloc((strlen(tmps12)+3)*sizeof(char));
      sprintf(pacId,"{%s}",tmps12);
      addParameter(conf,pacId,tmps12,"path",res9);
      free(pacId);
      tmps12 = strtok_r (NULL, ",", &saveptr12);
    }    
    tmpMap2=getMapFromMaps(conf,"openapi","header_parameters");
    if(tmpMap2!=NULL){
      char *saveptr13;
      char *tmps13 = strtok_r (tmpMap2->value, ",", &saveptr13);
      while(tmps13!=NULL){
	char* pacId=zStrdup(tmps13);
	addParameter(conf,pacId,pacId,"header",res9);
	free(pacId);
	tmps13 = strtok_r (NULL, ",", &saveptr13);
      }
    }
    json_object_object_add(res,"parameters",res9);
    maps* pmResponses=getMaps(conf,"responses");
    if(pmResponses!=NULL){
      json_object *cc=json_object_new_object();
      map* pmLen=getMap(pmResponses->content,"length");
      int iLen=atoi(pmLen->value);
      map* pmUseContent=getMapFromMaps(conf,"openapi","use_content");
      for(int i=0;i<iLen;i++){
	map* cMap=getMapArray(pmResponses->content,"code",i);
	map* vMap=getMapArray(pmResponses->content,"schema",i);
	map* tMap=getMapArray(pmResponses->content,"type",i);
	map* tMap0=getMapArray(pmResponses->content,"title",i);
	if(vMap!=NULL)
	  addResponse(pmUseContent,cc,vMap,tMap,cMap->value,(tMap0==NULL)?"successful operation":tMap0->value);
      }
      json_object_object_add(res,"responses",cc);
    }
  }

  void produceApiComponents(maps*conf,json_object* res){
    json_object* res1=json_object_new_object();
    produceApiParameters(conf,res1);
    json_object_object_add(res,"components",res1);
  }
  
  /**
   * Produce the JSON object for /api
   *
   * @param conf the maps containing the settings of the main.cfg file
   * @param res the JSON object to populate
   */
  void produceApi(maps* conf,json_object* res){
    // TODO: add 401 Gone for dismiss request
    json_object *res9=json_object_new_object();
    json_object *res10=json_object_new_object();
    maps* tmpMaps1=getMaps(conf,"{id}");
    produceApiComponents(conf,res);
    json_object *res4=json_object_new_object();
    setMapInMaps(conf,"headers","Content-Type","application/openapi+json; version=3.0;charset=UTF-8");

    produceApiInfo(conf,res,res4);
    map* tmpMap=getMapFromMaps(conf,"provider","providerName");

    tmpMap=getMapFromMaps(conf,"openapi","version");
    if(tmpMap!=NULL)
      json_object_object_add(res,"openapi",json_object_new_string(tmpMap->value));
    else
      json_object_object_add(res,"openapi",json_object_new_string("3.0.2"));

    tmpMap=getMapFromMaps(conf,"openapi","tags");
    if(tmpMap!=NULL){
      json_object *res5=json_object_new_array();
      char *saveptr;
      char *tmps = strtok_r (tmpMap->value, ",", &saveptr);
      while(tmps!=NULL){
	json_object *res6=json_object_new_object();
	json_object_object_add(res6,"name",json_object_new_string(tmps));
	json_object_object_add(res6,"description",json_object_new_string(tmps));
	json_object_array_add(res5,res6);
	tmps = strtok_r (NULL, ",", &saveptr);
      }
      json_object_object_add(res,"tags",res5);
    }

    tmpMap=getMapFromMaps(conf,"openapi","paths");
    if(tmpMap!=NULL){
      json_object *res5=json_object_new_object();
      char *saveptr;
      char *tmps = strtok_r (tmpMap->value, ",", &saveptr);
      int cnt=0;
      maps* tmpMaps;
      json_object *paths=json_object_new_object();
      while (tmps != NULL){
	tmpMaps=getMaps(conf,tmps+1);
	json_object *method=json_object_new_object();
	map* pmName=getMap(tmpMaps->content,"pname");
	if(tmpMaps!=NULL){
	  if(getMap(tmpMaps->content,"length")==NULL)
	    addToMap(tmpMaps->content,"length","1");
	  map* len=getMap(tmpMaps->content,"length");
	    
	  for(int i=0;i<atoi(len->value);i++){
	    json_object *methodc=json_object_new_object();
	    map* cMap=getMapArray(tmpMaps->content,"method",i);
	    map* vMap=getMapArray(tmpMaps->content,"title",i);
	    if(vMap!=NULL)
	      json_object_object_add(methodc,"summary",json_object_new_string(vMap->value));
	    vMap=getMapArray(tmpMaps->content,"abstract",i);
	    if(vMap!=NULL)
	      json_object_object_add(methodc,"description",json_object_new_string(vMap->value));
	    vMap=getMapArray(tmpMaps->content,"tags",i);
	    if(vMap!=NULL){
	      json_object *cc=json_object_new_array();
	      json_object_array_add(cc,json_object_new_string(vMap->value));
	      json_object_object_add(methodc,"tags",cc);
	      if(pmName!=NULL){
		char* pcaOperationId=(char*)malloc((strlen(vMap->value)+strlen(pmName->value)+1)*sizeof(char));
		sprintf(pcaOperationId,"%s%s",vMap->value,pmName->value);
		json_object_object_add(methodc,"operationId",json_object_new_string(pcaOperationId));
		free(pcaOperationId);
	      }
	      else
		json_object_object_add(methodc,"operationId",json_object_new_string(vMap->value));
	    }
	    json_object *responses=json_object_new_object();
	    json_object *cc3=json_object_new_object();
	    map* pmUseContent=getMapFromMaps(conf,"openapi","use_content");
	    vMap=getMapArray(tmpMaps->content,"schema",i);
	    if(vMap!=NULL){
	      map* tMap=getMapArray(tmpMaps->content,"type",i);
	      addResponse(pmUseContent,cc3,vMap,tMap,"200","successful operation");
	      vMap=getMapArray(tmpMaps->content,"eschema",i);
	      if(vMap!=NULL && cMap!=NULL && strncasecmp(cMap->value,"post",4)==0)
		addResponse(pmUseContent,cc3,vMap,tMap,"201","successful operation");
	    }else{
	      map* tMap=getMapFromMaps(conf,tmps,"type");
	      map* pMap=createMap("ok","true");
	      addResponse(pMap,cc3,vMap,tMap,"200","successful operation");
	      if(cMap!=NULL && strncasecmp(cMap->value,"post",4)==0)
		addResponse(pmUseContent,cc3,vMap,tMap,"201","successful operation");
	      freeMap(&pMap);
	      free(pMap);
	    }
	    vMap=getMapArray(tmpMaps->content,"ecode",i);
	    if(vMap!=NULL){
	      char *saveptr0;
	      char *tmps1 = strtok_r (vMap->value, ",", &saveptr0);
	      while(tmps1!=NULL){
		char* tmpStr=(char*)malloc((strlen(tmps1)+24)*sizeof(char));
		sprintf(tmpStr,"#/components/responses/%s",tmps1);
		vMap=createMap("ok",tmpStr);
		map* pMap=createMap("ok","false");
		//TODO: fix successful operation with correct value
		addResponse(pMap,cc3,vMap,NULL,tmps1,"successful operation");
		tmps1 = strtok_r (NULL, ",", &saveptr0);
	      }
	    }else{
	      if(strstr(tmps,"{id}")!=NULL){
		vMap=getMapFromMaps(conf,"exception","schema");
		map* tMap=getMapFromMaps(conf,"exception","type");
		if(vMap!=NULL)
		  addResponse(pmUseContent,cc3,vMap,tMap,"404",
			      (strstr(tmps,"{jobID}")==NULL)?"The process with id {id} does not exist.":"The process with id {id} or job with id {jobID} does not exist.");
	      }
	    }
	    json_object_object_add(methodc,"responses",cc3);
	    vMap=getMapArray(tmpMaps->content,"parameters",i);
	    if(vMap!=NULL){
	      char *saveptr0;
	      char *tmps1 = strtok_r (vMap->value, ",", &saveptr0);
	      json_object *cc2=json_object_new_array();
	      int cnt=0;
	      while(tmps1!=NULL){
		char* tmpStr=(char*)malloc((strlen(tmps1)+2)*sizeof(char));
		sprintf(tmpStr,"#%s",tmps1);
		json_object *cc1=json_object_new_object();
		json_object_object_add(cc1,"$ref",json_object_new_string(tmpStr));
		json_object_array_add(cc2,cc1);
		free(tmpStr);
		cnt++;
		tmps1 = strtok_r (NULL, ",", &saveptr0);
	      }
	      json_object_object_add(methodc,"parameters",cc2);
	    }
	    if(/*i==1 && */cMap!=NULL && strncasecmp(cMap->value,"post",4)==0){
	      maps* tmpMaps1=getMaps(conf,"requestBody");
	      if(tmpMaps1!=NULL){
		vMap=getMap(tmpMaps1->content,"schema");
		if(vMap!=NULL){
		  json_object *cc=json_object_new_object();
		  json_object_object_add(cc,"$ref",json_object_new_string(vMap->value));
		  json_object *cc0=json_object_new_object();
		  json_object_object_add(cc0,"schema",cc);
		  // Add examples from here
		  map* pmExample=getMap(tmpMaps->content,"examples");
		  if(pmExample!=NULL){
		    int iCnt=0;
		    char* saveptr;
		    map* pmExamplesPath=getMapFromMaps(conf,"openapi","examplesPath");
		    json_object* prop5=json_object_new_object();
		    char *tmps = strtok_r (pmExample->value, ",", &saveptr);
		    while(tmps!=NULL && strlen(tmps)>0){
		      json_object* prop6=json_object_new_object();
		      json_object* pjoExempleItem=json_object_new_object();
		      char* pcaExempleFile=NULL;
		      if(pmName!=NULL){
			pcaExempleFile=(char*)malloc((strlen(pmExamplesPath->value)+strlen(pmName->value)+strlen(tmps)+3)*sizeof(char));
			sprintf(pcaExempleFile,"%s/%s/%s",pmExamplesPath->value,pmName->value,tmps);
		      }else{
			pcaExempleFile=(char*)malloc((strlen(pmExamplesPath->value)+strlen(tmps)+2)*sizeof(char));
			sprintf(pcaExempleFile,"%s/%s",pmExamplesPath->value,tmps);
		      }
		      char actmp[10];
		      sprintf(actmp,"%d",iCnt);
		      char* pcaTmp;
		      map* pmSummary=getMapArray(tmpMaps->content,"examples_summary",iCnt);
		      if(pmSummary!=NULL){
			pcaTmp=(char*)malloc((strlen(actmp)+strlen(pmSummary->value)+strlen(_("Example "))+3)*sizeof(char));
			sprintf(pcaTmp,"%s%s: %s",_("Example "),actmp,pmSummary->value);		
		      }
		      else{
			pcaTmp=(char*)malloc((strlen(actmp)+strlen(_("Example "))+3)*sizeof(char));
			sprintf(pcaTmp,"%s%s",_("Example "),actmp);
		      }
		      json_object* pjoValue=json_readFile(conf,pcaExempleFile);
		      json_object_object_add(pjoExempleItem,"summary",json_object_new_string(pcaTmp));
		      json_object_object_add(pjoExempleItem,"value",pjoValue);
		      json_object_object_add(prop5,actmp,pjoExempleItem);
		      tmps = strtok_r (NULL, ",", &saveptr);
		      iCnt++;
		    }
		    json_object_object_add(cc0,"examples",prop5);
		  }
		  
		  json_object *cc1=json_object_new_object();
		  map* tmpMap3=getMap(tmpMaps->content,"type");
		  if(tmpMap3!=NULL)
		    json_object_object_add(cc1,tmpMap3->value,cc0);
		  else
		    json_object_object_add(cc1,"application/json",cc0);
		  json_object *cc2=json_object_new_object();
		  json_object_object_add(cc2,"content",cc1);
		  vMap=getMap(tmpMaps1->content,"abstract");
		  if(vMap!=NULL)
		    json_object_object_add(cc2,"description",json_object_new_string(vMap->value));
		  json_object_object_add(cc2,"required",json_object_new_boolean(true));

		  
		  json_object_object_add(methodc,"requestBody",cc2);

		}		
	      }
	      tmpMaps1=getMaps(conf,"callbacks");
	      if(tmpMaps1!=NULL){
		map* pmTmp2=getMap(tmpMaps1->content,"length");
		int iLen=atoi(pmTmp2->value);
		json_object *pajRes=json_object_new_object();
		for(int i=0;i<iLen;i++){
		  map* pmState=getMapArray(tmpMaps1->content,"state",i);
		  map* pmUri=getMapArray(tmpMaps1->content,"uri",i);
		  map* pmSchema=getMapArray(tmpMaps1->content,"schema",i);
		  map* pmType=getMapArray(tmpMaps1->content,"type",i);
		  map* pmTitle=getMapArray(tmpMaps1->content,"title",i);
		  json_object *pajSchema=json_object_new_object();
		  if(pmSchema!=NULL)
		    json_object_object_add(pajSchema,"$ref",json_object_new_string(pmSchema->value));
		  json_object *pajType=json_object_new_object();
		  json_object_object_add(pajType,"schema",pajSchema);
		  json_object *pajContent=json_object_new_object();
		  if(pmType!=NULL)
		    json_object_object_add(pajContent,pmType->value,pajType);
		  else		  
		    json_object_object_add(pajContent,"application/json",pajType);
		  json_object *pajRBody=json_object_new_object();
		  json_object_object_add(pajRBody,"content",pajContent);
		  
		  json_object *pajDescription=json_object_new_object();
		  json_object *pajPost=json_object_new_object();
		  if(pmTitle!=NULL){
		    json_object_object_add(pajDescription,"description",json_object_new_string(pmTitle->value));
		    json_object_object_add(pajPost,"summary",json_object_new_string(pmTitle->value));
		  }
		  json_object *pajResponse=json_object_new_object();
		  json_object_object_add(pajResponse,"200",pajDescription);

		  json_object_object_add(pajPost,"requestBody",pajRBody);
		  json_object_object_add(pajPost,"responses",pajResponse);
		  if(pmName!=NULL){
		    char* pcaOperationId=(char*)malloc((strlen(pmState->value)+strlen(pmName->value)+1)*sizeof(char));
		    sprintf(pcaOperationId,"%s%s",pmState->value,pmName->value);
		    json_object_object_add(pajPost,"operationId",json_object_new_string(pcaOperationId));
		    free(pcaOperationId);
		  }
		  else
		    json_object_object_add(pajPost,"operationId",json_object_new_string(pmState->value));
		  
		  json_object *pajMethod=json_object_new_object();
		  json_object_object_add(pajMethod,"post",pajPost);

		  
		  char* pacUri=(char*) malloc((strlen(pmUri->value)+29)*sizeof(char));
		  sprintf(pacUri,"{$request.body#/subscriber/%s}",pmUri->value);

		  json_object *pajFinal=json_object_new_object();
		  json_object_object_add(pajFinal,pacUri,pajMethod);
		  json_object_object_add(pajRes,pmState->value,pajFinal);

		}
		json_object_object_add(methodc,"callbacks",pajRes);
	      }
	    }
	    map* mMap=getMapArray(tmpMaps->content,"method",i);
	    if(mMap!=NULL)
	      json_object_object_add(method,mMap->value,methodc);
	    else
	      json_object_object_add(method,"get",methodc);

	  }

	  tmpMap=getMapFromMaps(conf,"openapi","version");
	  if(tmpMap!=NULL)
	    json_object_object_add(res,"openapi",json_object_new_string(tmpMap->value));
	  else
	    json_object_object_add(res,"openapi",json_object_new_string("3.0.2"));
	  if(strstr(tmps,"/root")!=NULL)
	    json_object_object_add(paths,"/",method);
	  else
	    json_object_object_add(paths,tmps,method);
	}
	tmps = strtok_r (NULL, ",", &saveptr);
	cnt++;
      }
      json_object_object_add(res,"paths",paths);
    }
      
    tmpMap=getMapFromMaps(conf,"openapi","links");
    if(tmpMap!=NULL){
	
    }
    
    json_object *res3=json_object_new_array();
    json_object_array_add(res3,res4);
    json_object_object_add(res,"servers",res3);
  }
  
#ifdef __cplusplus
}
#endif
