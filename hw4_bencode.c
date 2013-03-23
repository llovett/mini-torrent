#include<bencodetools/bencode.h>
#include<stdio.h>

void print_bencode(struct bencode* b) {
		 switch(b->type) {
		 case BENCODE_BOOL:
					printf("%s",((struct bencode_bool*)b)->b?"true":"false");
					break;
		 case BENCODE_DICT: {
			 struct bencode_dict *d = (struct bencode_dict*)b;
			 printf("{");
			 for(int i=0;i<d->n;i++) {
				 print_bencode(d->keys[i]);
				 printf(" : ");
				 print_bencode(d->values[i]);
				 printf(",");
			 }
			 printf("}\n");
			 break;
		 }
		 case BENCODE_INT:
			 printf("%lld",((struct bencode_int*)b)->ll);
			 break;
		 case BENCODE_LIST: {
			 struct bencode_dict *d = (struct bencode_dict*)b;
			 printf("{");
			 for(int i=0;i<d->n;i++) {
				 print_bencode(d->keys[i]);
				 printf(",");
			 }
			 printf("}");
			 break;
		 }
		 case BENCODE_STR:
			 printf("\"%s\"",((struct bencode_str*)b)->s);
			 break;
		 }
}

