/*###################################################################################################
 *      This file contains the code for generating the in-memory non-persistent filesystem - RAMDISK.
 *      Some code used here is picked up from various online resources.
 *      Please refer the REFERENCES file for the list of online resources used.
 *
 *      --------------------------------------
 *      Author: Ramya Vijayakumar
 *      --------------------------------------
 *###################################################################################################
*/

#include <string.h>
#include <libgen.h>

#include "ramnode.h"

void addNode(ramNode *head, ramNode *node) 
{
	node->next = NULL;

	if (head == NULL)
	{
		head = node;
	}
	else
	{
		ramNode *temp = head;
		while (temp->next != NULL)
		{
			temp = temp->next;
		}
		temp->next = node;
	}
}

/*void printLog(int fd, const char *msg) {
	write(fd, msg, strlen(msg));
}*/

ramNode* searchNode(ramNode *head, const char *name)
{
	ramNode *temp = head;
	while (temp != NULL && strcmp(name, temp->name) != 0)
	{
		temp = temp->next;
	}
	if (temp != NULL)
	{
		return temp;
	}
	else
	{
		return NULL;
	}
}

void printNodes(ramNode *head)
{
	printf("MKD: Nodes are: ");
	ramNode *temp = head;
	while (temp != NULL)
	{
		printf(" %s \n", temp->name);
		temp = temp->next;
	}
}

int deleteNode(ramNode *head, const char *path)
{
	ramNode *temp = head;
	ramNode *prev = temp;
	ramNode *foundTemp = NULL;
	ramNode *foundPrev = NULL;
	char *tempName;
	
	while (temp != NULL)
	{
		tempName = (char *) malloc(temp->size);
		strcpy(tempName, temp->name);

		char *dirName = dirname(tempName);
		// Check whether it has any children, If so, throw an error
		if (strcmp(dirName, path) == 0)
		{
			return -ENOTEMPTY;
		}
		else if(strcmp(temp->name, path) == 0 )
		{
			foundTemp = temp;
			foundPrev = prev;
		}
		prev = temp;
		temp = temp->next;
	}
	if(foundTemp == NULL)
	{
		return -ENOENT;
	}
	else
	{
		foundPrev->next = foundTemp->next;
		return 0;
	}
}

int deleteFile(ramNode *head, const char *path)
{
	ramNode *temp = head;
	ramNode *prev = temp;
	ramNode *foundTemp = NULL;
	ramNode *foundPrev = NULL;
	
	while (temp != NULL)
	{

		// Check whether it has any children, If so, throw an error
		if(strcmp(temp->name, path) == 0 )
		{
			foundTemp = temp;
			foundPrev = prev;
		}
		prev = temp;
		temp = temp->next;
	}
	if(foundTemp == NULL)
	{
		return -ENOENT;
	}
	else
	{
		foundPrev->next = foundTemp->next;
		return 0;
	}
}

